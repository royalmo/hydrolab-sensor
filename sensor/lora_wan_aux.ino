// Auxiliary LoRaWAN methods that I need to define but I don't care

void onEvent (ev_t ev) {
    switch(ev) {
//        case EV_SCAN_TIMEOUT:
//            Serial.println(F("EV_SCAN_TIMEOUT"));
//            break;
        case EV_JOINING:
            Serial.println(F("JOINING"));
            break;
        case EV_JOINED:
            Serial.println(F("JOINED!!"));
            // Disable link check validation (automatically enabled
            // during join, but because slow data rates change max TX
            // size, we don't use it in this example.
            LMIC_setLinkCheckMode(0);

            
            digitalWrite(LED_BLUE, LOW);
            digitalWrite(LED_GREEN, LOW);
            delay(200);
            digitalWrite(LED_BLUE, HIGH);
            digitalWrite(LED_GREEN, HIGH);
            break;
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            break;
        case EV_TXCOMPLETE:
            Serial.println(F("TXCOMPLETE!!")); // (includes waiting for RX windows)

            digitalWrite(LED_GREEN, LOW);
            delay(200);
            digitalWrite(LED_GREEN, HIGH);

            failed_attempts = 0;
            first_message_sent = true;

            //if (LMIC.txrxFlags & TXRX_ACK)
            //  Serial.println(F("Received ack"));
            if (LMIC.dataLen) {
              Serial.print(F("Received "));
              int i;
              for (i=0; i<LMIC.dataLen; i++) {
                Serial.print((char) LMIC.frame[i+15]);
                payload[i] = LMIC.frame[i+15];
              }
              payload[i] = '\0';
              Serial.println("");

              process_downlink();
            }

            if (!interval_set) {
              interval_set = true;
              // Schedule next transmission
              int tx_interval = get_tx_interval();
              os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(tx_interval), do_send);
              // Commented due to performance reasons.
              // It will fail sometimes due to low stack sizes.
              //if (tx_interval >= 120) {
                //Watchdog.sleep(tx_interval);
              //}
            }
            break;
        case EV_RXCOMPLETE:
            // data received in ping slot
            Serial.println(F("EV_RXCOMPLETE"));
            break;
        case EV_JOIN_TXCOMPLETE:
            Serial.println(F("Retrying Join in 1'"));
            digitalWrite(LED_RED, LOW);
            delay(200);
            digitalWrite(LED_RED, HIGH);
            
            if (++failed_attempts >= 5) {
              decide_on_my_own();
            }
            
            break;
        default:
            break;
    }
}

void user_request_network_time_callback(void *pVoidUserUTCTime, int flagSuccess) {
    // Explicit conversion from void* to uint32_t* to avoid compiler errors
    uint32_t *pUserUTCTime = (uint32_t *) pVoidUserUTCTime;

    // A struct that will be populated by LMIC_getNetworkTimeReference.
    // It contains the following fields:
    //  - tLocal: the value returned by os_GetTime() when the time
    //            request was sent to the gateway, and
    //  - tNetwork: the seconds between the GPS epoch and the time
    //              the gateway received the time request
    lmic_time_reference_t lmicTimeReference;

    if (flagSuccess != 1) {
        //Serial.println(F("USER CALLBACK: Not a success"));
        return;
    }

    // Populate "lmic_time_reference"
    flagSuccess = LMIC_getNetworkTimeReference(&lmicTimeReference);
    if (flagSuccess != 1) {
        //Serial.println(F("USER CALLBACK: LMIC_getNetworkTimeReference didn't succeed"));
        return;
    }

    // Update userUTCTime, considering the difference between the GPS and UTC
    // epoch, and the leap seconds
    *pUserUTCTime = lmicTimeReference.tNetwork + 315964800;

    // Add the delay between the instant the time was transmitted and
    // the current time

    // Current time, in ticks
    ostime_t ticksNow = os_getTime();
    // Time when the request was sent, in ticks
    ostime_t ticksRequestSent = lmicTimeReference.tLocal;
    uint32_t requestDelaySec = osticks2ms(ticksNow - ticksRequestSent) / 1000;
    *pUserUTCTime += requestDelaySec;

    // Update the system time with the time read from the network
    setTime(*pUserUTCTime);
    Serial.println("Time updated");
    time_updated = true;
}
