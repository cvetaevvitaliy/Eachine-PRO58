#include "settings.h"

#include "settings_eeprom.h"
#include "receiver.h"
#include "receiver_spi.h"
#include "channels.h"

#include "timer.h"

#define INVERT_LOGIC(x) x=(x == GPIO_PIN_SET ? GPIO_PIN_RESET : GPIO_PIN_SET)

#define abs(x) (((x) > 0) ? (x) : -(x))
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))

static inline uint16_t map(uint16_t value, uint16_t fromStart, uint16_t fromEnd,
		uint16_t toStart, uint16_t toEnd);


namespace Receiver {
	uint16_t rssiDMARaw[2]; //0-A 1-B

	uint16_t rssiRawA[1], rssiRawB[1];

    ReceiverId activeReceiver = ReceiverId::A;
    uint8_t activeChannel = 0;

    uint8_t rssiA = 0;
    uint16_t rssiARaw = 0;
    uint8_t rssiALast[RECEIVER_LAST_DATA_SIZE] = { 0 };
    #ifdef USE_DIVERSITY
        uint8_t rssiB = 0;
        uint16_t rssiBRaw = 0;
        uint8_t rssiBLast[RECEIVER_LAST_DATA_SIZE] = { 0 };

        ReceiverId diversityTargetReceiver = activeReceiver;
        Timer diversityHysteresisTimer = Timer(DIVERSITY_HYSTERESIS_PERIOD);
    #endif

    static Timer rssiStableTimer = Timer(MIN_TUNE_TIME);
    static Timer rssiLogTimer = Timer(RECEIVER_LAST_DELAY);
    #ifdef USE_SERIAL_OUT
        static Timer serialLogTimer = Timer(25);
    #endif

    void init(ADC_HandleTypeDef *hadc){
    	HAL_ADC_Start_DMA(hadc, (uint32_t*)rssiDMARaw, 2);
    }

    void setChannel(uint8_t channel, bool store)
    {
        ReceiverSpi::setSynthRegisterB(Channels::getSynthRegisterB(channel));

        rssiStableTimer.reset();
        activeChannel = channel;
        if (store){
            EepromSettings.startChannel = channel;
            EepromSettings.markDirty();
        }
    }

    void setBandChannel(uint8_t inChannel, bool store)
    {
        uint8_t bandOffset = activeChannel - (activeChannel % 8);
        uint8_t orderedChanelIndex = bandOffset + inChannel;
        setChannel(orderedChanelIndex, store);
    }

    void setActiveReceiver(ReceiverId receiver) {
    	bool swState = receiver == ReceiverId::A;
    	bool ledAState = receiver != ReceiverId::A;
    	bool ledBState = receiver != ReceiverId::B;

#ifdef HB5808 //HB5808 has inverted logic in video switch and leds.
    	swState = !swState;
    	ledAState = !ledAState;
    	ledBState = !ledBState;
#endif

		HAL_GPIO_WritePin(LED_A_GPIO_Port,LED_A_Pin, ledAState ? GPIO_PIN_SET : GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LED_B_GPIO_Port,LED_B_Pin, ledBState ? GPIO_PIN_SET : GPIO_PIN_RESET);
		HAL_GPIO_WritePin(RECEIVER_SW_GPIO_Port, RECEIVER_SW_Pin, swState ? GPIO_PIN_SET : GPIO_PIN_RESET);

    
        activeReceiver = receiver;
    }

    bool isRssiStable() {
        return rssiStableTimer.hasTicked();
    }

    void updateRssi() {
        rssiARaw = rssiDMARaw[0];
        rssiRawA[0] = rssiARaw;
        #ifdef USE_DIVERSITY
            rssiBRaw = rssiDMARaw[1];
            rssiRawB[0] = rssiBRaw;
        #endif

        rssiA = constrain(
            map(
                rssiARaw,
                EepromSettings.rssiAMin,
                EepromSettings.rssiAMax,
                0,
                100
            ),
            0,
            100
        );
        #ifdef USE_DIVERSITY
            rssiB = constrain(
                map(
                    rssiBRaw,
                    EepromSettings.rssiBMin,
                    EepromSettings.rssiBMax,
                    0,
                    100
                ),
                0,
                100
            );
        #endif

        if(rssiA == 100 || rssiB == 100){
        	uint8_t i = 0;
        }

        if (rssiLogTimer.hasTicked()) {
            for (uint8_t i = 0; i < RECEIVER_LAST_DATA_SIZE - 1; i++) {
                rssiALast[i] = rssiALast[i + 1];
                #ifdef USE_DIVERSITY
                    rssiBLast[i] = rssiBLast[i + 1];
                #endif
            }

            rssiALast[RECEIVER_LAST_DATA_SIZE - 1] = rssiA;
            #ifdef USE_DIVERSITY
                rssiBLast[RECEIVER_LAST_DATA_SIZE - 1] = rssiB;
            #endif

            rssiLogTimer.reset();
        }
    }

#ifdef USE_DIVERSITY
    void setDiversityMode(DiversityMode mode) {
        EepromSettings.diversityMode = mode;
        switchDiversity();
    }

    void switchDiversity() {
        ReceiverId nextReceiver = activeReceiver;

        if (EepromSettings.diversityMode == DiversityMode::AUTO) {
            int8_t rssiDiff = (int8_t) rssiA - (int8_t) rssiB;
            uint8_t rssiDiffAbs = abs(rssiDiff);
            ReceiverId currentBestReceiver = activeReceiver;

            if (rssiDiff > 0) {
                currentBestReceiver = ReceiverId::A;
            } else if (rssiDiff < 0) {
                currentBestReceiver = ReceiverId::B;
            } else {
                currentBestReceiver = activeReceiver;
            }

            if (rssiDiffAbs >= DIVERSITY_HYSTERESIS) {
                if (currentBestReceiver == diversityTargetReceiver) {
                    if (diversityHysteresisTimer.hasTicked()) {
                        nextReceiver = diversityTargetReceiver;
                    }
                } else {
                    diversityTargetReceiver = currentBestReceiver;
                    diversityHysteresisTimer.reset();
                }
            } else {
                diversityHysteresisTimer.reset();
            }
        } else {
            switch (EepromSettings.diversityMode) {
                case DiversityMode::FORCE_A:
                    nextReceiver = ReceiverId::A;
                    break;

                case DiversityMode::FORCE_B:
                    nextReceiver = ReceiverId::B;
                    break;
                default:
                	EepromSettings.diversityMode = DiversityMode::AUTO;
            }
        }

        setActiveReceiver(nextReceiver);
    }
#endif

    void setup(ADC_HandleTypeDef *adc_handle) {
    	HAL_ADC_Start_DMA(adc_handle, (uint32_t*)rssiDMARaw, 2);

    	if (EepromSettings.audioEnabled == 0)
        ReceiverSpi::setPowerDownRegister(0b00010000110111110011);


        #ifdef DISABLE_AUDIO
            ReceiverSpi::setPowerDownRegister(0b00010000110111110011);
        #endif
    }

    void update() {
        if (rssiStableTimer.hasTicked()) {
            updateRssi();

//            #ifdef USE_SERIAL_OUT
//                writeSerialData();
//            #endif

            #ifdef USE_DIVERSITY
                switchDiversity();
            #endif
        }
    }
}

static inline uint16_t map(uint16_t value, uint16_t fromStart, uint16_t fromEnd,
		uint16_t toStart, uint16_t toEnd) {
	int16_t val = (value - fromStart);
    return (val >= 0 ? val : 0) * (toEnd - toStart) / (fromEnd - fromStart) +
        toStart;
}
//#ifdef USE_SERIAL_OUT
//
//#include "pstr_helper.h"
//
//static void writeSerialData() {
//    if (Receiver::serialLogTimer.hasTicked()) {
//        Serial.print(Receiver::activeChannel, DEC);
//        Serial.print(PSTR2("\t"));
//        Serial.print(Receiver::rssiA, DEC);
//        Serial.print(PSTR2("\t"));
//        Serial.print(Receiver::rssiARaw, DEC);
//        Serial.print(PSTR2("\t"));
//        Serial.print(Receiver::rssiB, DEC);
//        Serial.print(PSTR2("\t"));
//        Serial.println(Receiver::rssiBRaw, DEC);
//
//        Receiver::serialLogTimer.reset();
//    }
//}
//#endif
