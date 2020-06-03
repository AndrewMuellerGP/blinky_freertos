/*
 * Copyright 2015-2020 Texas Instruments Incorporated. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
/*
 * TODO: Get definition of struct timeval from sys/select.h (workaround for
 * NS-93)
 */
#include <sys/select.h>

#include <errno.h>

#include <ti/net/slnetsock.h>
#include <ti/net/slnetif.h>

#include <network_interface.h>
#include <aws_iot_error.h>
#include <aws_iot_log.h>

extern void checkIfFileExists(uint8_t *certName);

extern uint32_t NetWiFi_isConnected(void);

static void iot_tls_set_connect_params(Network *pNetwork, char *pRootCALocation,
        char *pDeviceCertLocation, char *pDevicePrivateKeyLocation,
        char *pDestinationURL, uint16_t DestinationPort, uint32_t timeout_ms,
        bool ServerVerificationFlag) {

    pNetwork->tlsConnectParams.DestinationPort = DestinationPort;
    pNetwork->tlsConnectParams.pDestinationURL = pDestinationURL;
    pNetwork->tlsConnectParams.pDeviceCertLocation = pDeviceCertLocation;
    pNetwork->tlsConnectParams.pDevicePrivateKeyLocation =
            pDevicePrivateKeyLocation;
    pNetwork->tlsConnectParams.pRootCALocation = pRootCALocation;
    pNetwork->tlsConnectParams.timeout_ms = timeout_ms;
    pNetwork->tlsConnectParams.ServerVerificationFlag = ServerVerificationFlag;
}

IoT_Error_t iot_tls_init(Network *pNetwork, char *pRootCALocation,
        char *pDeviceCertLocation, char *pDevicePrivateKeyLocation,
        char *pDestinationURL, uint16_t DestinationPort, uint32_t timeout_ms,
        bool ServerVerificationFlag)
{
    FUNC_ENTRY;

    if (pNetwork == NULL) {
        FUNC_EXIT_RC(NULL_VALUE_ERROR);
    }

    iot_tls_set_connect_params(pNetwork, pRootCALocation, pDeviceCertLocation,
            pDevicePrivateKeyLocation, pDestinationURL, DestinationPort,
            timeout_ms, ServerVerificationFlag);

    pNetwork->connect = iot_tls_connect;
    pNetwork->read = iot_tls_read;
    pNetwork->write = iot_tls_write;
    pNetwork->disconnect = iot_tls_disconnect;
    pNetwork->isConnected = iot_tls_is_connected;
    pNetwork->destroy = iot_tls_destroy;

    pNetwork->tlsDataParams.ifId = 0; /* init to invalid interface ID */
    pNetwork->tlsDataParams.skt = -1; /* INVALID socket */
    pNetwork->tlsDataParams.secAttrib = NULL;

    FUNC_EXIT_RC(SUCCESS);
}

IoT_Error_t iot_tls_connect(Network *pNetwork, TLSConnectParams *TLSParams)
{
    IoT_Error_t ret = SUCCESS;
    int status = 0;
    uint32_t certStore;
    uint8_t  securityMethod;
    uint32_t securityCipher;
    struct addrinfo hints;
    struct addrinfo *results = NULL;
    struct addrinfo *currAddr = NULL;
    char portStr[6]; /* 6 chars needed to hold a short (max 5 digits) + '\0' */
    TLSConnectParams *tlsParams;
    TLSDataParams *tlsDataParams;
    uint16_t clientSd;
    socklen_t sdlen = sizeof(clientSd);

    FUNC_ENTRY;

    if (pNetwork == NULL) {
        FUNC_EXIT_RC(NULL_VALUE_ERROR);
    }

    if (TLSParams != NULL) {
        iot_tls_set_connect_params(pNetwork, TLSParams->pRootCALocation,
                TLSParams->pDeviceCertLocation,
                TLSParams->pDevicePrivateKeyLocation,
                TLSParams->pDestinationURL, TLSParams->DestinationPort,
                TLSParams->timeout_ms, TLSParams->ServerVerificationFlag);
    }

    /* Use TLS params in Network struct */
    tlsParams = &pNetwork->tlsConnectParams;
    tlsDataParams = &pNetwork->tlsDataParams;

    /* Convert the AWS server's port number to a string */
    status = sprintf(portStr, "%d", tlsParams->DestinationPort);
    if (status < 0) {
        ret = FAILURE;
        goto QUIT;
    }

    /*
     * Perform host name look up on the AWS server name.
     * We need to connect using TCP over either IPv4 or IPv6
     */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    status = getaddrinfo(tlsParams->pDestinationURL, portStr, &hints, &results);
    if (status != 0) {
        IOT_ERROR("getaddrinfo returned %d\n", status);
        ret = NETWORK_ERR_NET_UNKNOWN_HOST;
        goto QUIT;
    }

    /* Cycle over results, use the first socket that connects successfully */
    for (currAddr = results; currAddr != NULL; currAddr = currAddr->ai_next) {
        tlsDataParams->skt = socket(currAddr->ai_family, currAddr->ai_socktype,
                currAddr->ai_protocol);
        if (tlsDataParams->skt < 0) {
            /* socket failed; try again with the next result */
            ret = NETWORK_ERR_NET_SOCKET_FAILED;
            continue;
        }

        status = connect(tlsDataParams->skt, currAddr->ai_addr,
                currAddr->ai_addrlen);
        if (status < 0) {
            /* connect failed for this address, try the next one */
            close(tlsDataParams->skt);
            tlsDataParams->skt = -1;
            ret = NETWORK_ERR_NET_CONNECT_FAILED;
        }
        else {
            /* connect succeeded, we're done */
            ret = SUCCESS;
            break;
        }
    }

    /* If we couldn't create and/or connect a socket, quit with failure */
    if (ret != SUCCESS) {
        IOT_ERROR("Failed to create and/or connect a socket\n");
        goto QUIT;
    }

    if (getsockopt(tlsDataParams->skt, SLNETSOCK_LVL_SOCKET,
            SLNETSOCK_OPSOCK_SLNETSOCKSD, &clientSd, &sdlen) < 0) {
        IOT_ERROR("getsockopt failed (errno = %d)\n", errno);
        ret = TCP_SETUP_ERROR;
        goto QUIT;
    }

    /* Get and save the associated interface ID for this socket */
    tlsDataParams->ifId = SlNetSock_getIfID(tlsDataParams->skt);
    if (tlsDataParams->ifId < 0) {
        IOT_ERROR("SlNetSock_getIfID retruned %d\n", status);
        ret = TCP_SETUP_ERROR;
        goto QUIT;
    }

    tlsDataParams->secAttrib = SlNetSock_secAttribCreate();
    if (!(tlsDataParams->secAttrib)) {
        IOT_ERROR("SlNetSock_secAttribCreate failed\n");
        ret = NETWORK_SSL_INIT_ERROR;
        goto QUIT;
    }

    /* Set the root CA certificate */
    status = SlNetSock_secAttribSet(tlsDataParams->secAttrib,
            SLNETSOCK_SEC_ATTRIB_PEER_ROOT_CA,
            tlsParams->pRootCALocation,
            strlen(tlsParams->pRootCALocation) + 1);

    /* Set this device's private key */
    status |= SlNetSock_secAttribSet(tlsDataParams->secAttrib,
            SLNETSOCK_SEC_ATTRIB_PRIVATE_KEY,
            tlsParams->pDevicePrivateKeyLocation,
            strlen(tlsParams->pDevicePrivateKeyLocation) + 1);

    /* Set this device's certificate */
    status |= SlNetSock_secAttribSet(tlsDataParams->secAttrib,
            SLNETSOCK_SEC_ATTRIB_LOCAL_CERT,
            tlsParams->pDeviceCertLocation,
            strlen(tlsParams->pDeviceCertLocation) + 1);

    /* Disable the cert store */
    certStore = 1;
    status |= SlNetSock_secAttribSet(tlsDataParams->secAttrib,
           SLNETSOCK_SEC_ATTRIB_DISABLE_CERT_STORE, (void *)&certStore,
           sizeof(certStore));

    securityMethod = SLNETSOCK_SEC_METHOD_SSLv3_TLSV1_2;
    status |= SlNetSock_secAttribSet(tlsDataParams->secAttrib,
           SLNETSOCK_SEC_ATTRIB_METHOD, (void *)&(securityMethod),
           sizeof(securityMethod));

    securityCipher = SLNETSOCK_SEC_CIPHER_FULL_LIST;
    status |= SlNetSock_secAttribSet(tlsDataParams->secAttrib,
            SLNETSOCK_SEC_ATTRIB_CIPHERS, (void *)&(securityCipher),
            sizeof(securityCipher));

    /* Consolidate error checking for SlNetSock_secAttribSet calls here: */
    if (status < 0) {
        IOT_ERROR("SlNetSock_secAttribSet retruned %d\n", status);
        ret = NETWORK_SSL_INIT_ERROR;
        goto QUIT;
    }

    /* Bind the TLS context */
    status = SlNetSock_startSec(clientSd, tlsDataParams->secAttrib,
           SLNETSOCK_SEC_BIND_CONTEXT_ONLY);
    if (status < 0) {
        IOT_ERROR("SlNetSock_startSec retruned %d\n", status);
        ret = NETWORK_SSL_INIT_ERROR;
        goto QUIT;
    }

    /* Start the TLS handshake */
    status = SlNetSock_startSec(clientSd, NULL,
           SLNETSOCK_SEC_START_SECURITY_SESSION_ONLY);
    if (status < 0) {
        IOT_ERROR("SlNetSock_startSec retruned %d\n", status);
        switch (status) {
            /* Problem with root CA cert */
            case SLNETERR_ESEC_BAD_CA_FILE:
                ret = NETWORK_X509_ROOT_CRT_PARSE_ERROR;
                break;

            /* Problem with device cert */
            case SLNETERR_ESEC_BAD_CERTIFICATE:
            case SLNETERR_ESEC_BAD_CERT_FILE:
                ret = NETWORK_X509_DEVICE_CRT_PARSE_ERROR;
                break;

            /* Problem with private key */
            case SLNETERR_ESEC_BAD_PRIVATE_FILE:
                ret = NETWORK_PK_PRIVATE_KEY_PARSE_ERROR;
                break;

            /* SSL Handshake time out */
            case SLNETERR_ESEC_HAND_SHAKE_TIMED_OUT:
                ret = NETWORK_SSL_CONNECT_TIMEOUT_ERROR;
                break;

            default :
                ret = SSL_CONNECTION_ERROR;
                break;
        }
    }

/*
 * Clean up and return
 */
QUIT:

    if (results) {
        freeaddrinfo(results);
    }

    if (ret != SUCCESS) {
        if (tlsDataParams->secAttrib) {
            SlNetSock_secAttribDelete(tlsDataParams->secAttrib);
            tlsDataParams->secAttrib = NULL;
        }

        if (tlsDataParams->skt >= 0) {
            close(tlsDataParams->skt);
            tlsDataParams->skt = -1;
        }
    }

    FUNC_EXIT_RC(ret);
}

IoT_Error_t iot_tls_is_connected(Network *pNetwork)
{
    int32_t status;
    IoT_Error_t ret;

    FUNC_ENTRY;

    status = SlNetIf_getConnectionStatus(pNetwork->tlsDataParams.ifId);

    if (status == SLNETIF_STATUS_CONNECTED) {
        ret = NETWORK_PHYSICAL_LAYER_CONNECTED;
    }
    else {
        /*
         * Although SlNetIf_getConnectionStatus() has more possible failure
         * codes, the AWS framework code only checks for success (i.e.
         * "connnected") when this function is called. Therefore, return
         * "disconnected" here generically:
         */
        ret = NETWORK_PHYSICAL_LAYER_DISCONNECTED;
    }

    FUNC_EXIT_RC(ret);
}

IoT_Error_t iot_tls_write(Network *pNetwork, unsigned char *pMsg, size_t len,
            Timer *timer, size_t *numbytes)
{
    int bytes = 0;

    FUNC_ENTRY;

    if (pNetwork == NULL || pMsg == NULL ||
            pNetwork->tlsDataParams.skt == -1 || numbytes == NULL) {
        FUNC_EXIT_RC(NULL_VALUE_ERROR);
    }

    bytes = send(pNetwork->tlsDataParams.skt, pMsg, len, 0);
    if (bytes > 0) {
        *numbytes = (size_t)bytes;
        FUNC_EXIT_RC(SUCCESS);
    }
    else if (bytes < 0) {
        IOT_ERROR("send failed (errno = %d)\n", errno);
    }

    FUNC_EXIT_RC(NETWORK_SSL_WRITE_ERROR);
}

IoT_Error_t iot_tls_read(Network *pNetwork, unsigned char *pMsg, size_t len,
        Timer *timer, size_t *numbytes)
{
    int bytesLeft;
    int bytesRcvd = 0;
    int totalBytes = 0;
    int skt;
    struct timeval tv;
    uint32_t timeout;

    FUNC_ENTRY;

    if (pNetwork == NULL || pMsg == NULL ||
            pNetwork->tlsDataParams.skt == -1 || timer == NULL ||
            numbytes == NULL) {
        FUNC_EXIT_RC(NULL_VALUE_ERROR);
    }

    timeout = left_ms(timer);
    if (timeout == 0) {
        /* sock timeout of 0 == block forever; just read + return if expired */
        timeout = 1;
    }

    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    skt = pNetwork->tlsDataParams.skt;

    if (setsockopt(skt, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,
            sizeof(struct timeval)) == 0) {

        /* Receive all bytes requested */
        bytesLeft = len;
        while (bytesLeft > 0) {
            bytesRcvd = recv(skt, pMsg, bytesLeft, 0);

            if (bytesRcvd > 0) {
                bytesLeft -= bytesRcvd;
                totalBytes += bytesRcvd;
                pMsg += bytesRcvd;
            }
            else if (bytesRcvd < 0 && errno != EAGAIN) {
                IOT_ERROR("recv failed (errno = %d)\n", errno);
                break;
            }
            else {
                /* recv() returned 0 */
                break;
            }
        }
        /* Report back what was received, even if an error occurred. */
        *numbytes = (size_t)totalBytes;
        if (totalBytes == len) {
            /* All requested bytes received, success */
            FUNC_EXIT_RC(SUCCESS);
        }
        else if ((bytesRcvd == -1) && (errno == EAGAIN)) {
            /* nothing to read in the socket buffer */
            FUNC_EXIT_RC(NETWORK_SSL_NOTHING_TO_READ);
        }
        /* else recv() failed or returned 0, fall through */
    }
    FUNC_EXIT_RC(NETWORK_SSL_READ_ERROR);
}

IoT_Error_t iot_tls_disconnect(Network *pNetwork)
{
    /*
     * SlNetSock has no disconnect equivalent. AWS framework code always
     * calls disconnect()/destroy() back to back so OK to have this empty.
     */
    return (SUCCESS);
}

IoT_Error_t iot_tls_destroy(Network *pNetwork)
{
    FUNC_ENTRY;

    if (pNetwork == NULL) {
        FUNC_EXIT_RC(NULL_VALUE_ERROR);
    }

    if (pNetwork->tlsDataParams.skt >= 0) {
        close(pNetwork->tlsDataParams.skt);
        pNetwork->tlsDataParams.skt = -1;
    }

    if (pNetwork->tlsDataParams.secAttrib) {
        SlNetSock_secAttribDelete(pNetwork->tlsDataParams.secAttrib);
        pNetwork->tlsDataParams.secAttrib = NULL;
    }

    FUNC_EXIT_RC(SUCCESS);
}
