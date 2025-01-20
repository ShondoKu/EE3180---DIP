/**
 **************************************************
 *
 * @file        PMS7003-SOLDERED.h
 * @brief       Wrapper for our board.
 *
 *
 * @copyright GNU General Public License v3.0
 * @authors     Zvonimir Haramustek for www.soldered.com
 ***************************************************/

#ifndef __PMS7003_SOLDERED__
#define __PMS7003_SOLDERED__

#include "libs/PMserial/src/PMserial.h"

#include "Arduino.h"

#undef PMS7003

class PMS7003 : public SerialPM
{
  public:
    PMS7003(int _pin1, int _pin2) : SerialPM(PLANTOWER_32B, _pin1, _pin2)
    {
    }

    void begin()
    {
        init();
    }
};

#endif
