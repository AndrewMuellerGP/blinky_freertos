/**
 * This routine shows how to retrieve scan results form the NWP.
 *
 * @param arg - Points to command line buffer.
 *              This container would be passed
 *              to the parser module.
 *
 * @return Upon successful completion, the function shall return 0.
 *         In case of failure, this function would return -1;
 *
 * @note  If scans aren't active, this function triggers one scan
 *        and later prints the results.
 *
 */
int32_t cmdScanCallback(void *arg)
{
    int32_t ret = -1;
    uint8_t triggeredScanTrials = 0;
    ScanCmd_t ScanParams;

    /* Call the command parser */
    memset(&ScanParams, 0x0, sizeof(ScanParams));
    ret = ParseScanCmd(arg, &ScanParams);

    if(ret < 0)
    {
        return(-1);
    }

    /* Clear the results buffer */
    memset(&app_CB.gDataBuffer, 0x0, sizeof(app_CB.gDataBuffer));

    if(ScanParams.extendedRes)
    {
        ret = sl_WlanGetExtNetworkList
                  (ScanParams.index,ScanParams.numOfentries,
                  &app_CB.gDataBuffer.extNetEntries[ScanParams.index]);
    }
    else
    {
        /* Get scan results from NWP -
           results would be placed inside the provided buffer */
        ret = sl_WlanGetNetworkList(ScanParams.index,
                                    ScanParams.numOfentries,
                                    &app_CB.gDataBuffer.netEntries[ScanParams.
                                                                   index]);
    }

    /* If scan policy isn't set, invoking 'sl_WlanGetNetworkList()'
     * for the first time triggers 'one shot' scan.
     * The scan parameters would be according to the system persistent
     * settings on enabled channels.
     * For more information, see: <simplelink user guide, page: pr.>
     */
    if(SL_ERROR_WLAN_GET_NETWORK_LIST_EAGAIN == ret)
    {
        while(triggeredScanTrials < MAX_SCAN_TRAILS)
        {
            /* We wait for one second for the NWP to complete
               the initiated scan and collect results */
            sleep(1);

            /* Collect results form one-shot scans.*/
            if(ScanParams.extendedRes)
            {
                ret = sl_WlanGetExtNetworkList
                          (ScanParams.index,ScanParams.numOfentries,
                          &app_CB.gDataBuffer.extNetEntries[ScanParams.index]);
            }
            else
            {
                /* Get scan results from NWP -
                   results would be placed inside the provided buffer */
                ret = sl_WlanGetNetworkList(ScanParams.index,
                                            ScanParams.numOfentries,
                                            &app_CB.gDataBuffer.netEntries[
                                                ScanParams.index]);
            }
            if(ret > 0)
            {
                break;
            }
            else
            {
                /* If NWP results aren't ready,
                   try 'MAX_SCAN_TRAILS' attempts to get results */
                triggeredScanTrials++;
            }
        }
    }

    if(ret <= 0)
    {
        UART_PRINT("\n\r[scan] : Unable to retrieve the network list\n\r");
        return(-1);
    }
    /* Print the result table */
    if(ScanParams.extendedRes)
    {
        printExtScanResults(ret);
    }
    else
    {
        printScanResults(ret);
    }

    return(ret);
}