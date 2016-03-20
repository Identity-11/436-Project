#include "SparkFunLSM9DS1/SparkFunLSM9DS1.h"
#include "math.h"

#define LOOP_INTERVAL         1000 /* In Milliseconds */
#define EMAIL_INTERVAL        60   /* In Seconds */

#define TUCSON_MAGNETIC_PULL  9.59 /* In m/s */

#define AWAITING_START_STATE  1
#define WASHING_STATE         2
#define AWAITING_DOOR_STATE   3

#define DEBUG_STATE           1

#define MOTION_PADDING        10

LSM9DS1 imu;

float calcMagnitude();
float findRestingChange();
byte   threshold();

int 	currentState;

long 	totalTime 	= 0;
int 	windowStart = 0;

byte 	window[60];

float 	lastMagnitude = 0.0F;
float	restingMagnitudeChange;

void setup()
{
	Serial.begin(115200);

	imu.settings.device.commInterface = 	IMU_MODE_I2C;
    imu.settings.device.agAddress     = 	0x6B;
    imu.settings.device.mAddress      = 	0x1E;

    imu.begin();

    currentState = AWAITING_START_STATE;

    restingMagnitudeChange = findRestingChange();

    for (int i = 0; i < 60; i++)
    {
    	window[i] = 0;
    }

    while (1)
    {
    	loop();
    	delay(LOOP_INTERVAL);
    }
}

void loop()
{
	if (currentState == AWAITING_START_STATE)
	{
		int i;
		int sum = 0;

		for (i = 0; i < 59; i++)
		{
			sum += window[i];
			window[i] = window[i+1];
		}
		window[59] = threshold();
		sum += window[59];

		if (sum >= 45) /* 75% of the last minute detected motion */
		{
			currentState = WASHING_STATE;
			for (i = 0; i < 60; i++)
			{
				window[i] = 1;
			}
		}
	}

	if (currentState == WASHING_STATE)
	{
		int i = 0;
		int sum = 0;

		for (i = 0; i < 59; i++)
		{
			sum += window[i];
			window[i] = window[i+1];
		}
		window[59] = threshold();
		sum += window[59];

		if (sum <= 10) /* 83.33% of last minute, no motion detected */
		{ 
			currentState = AWAITING_DOOR_STATE;
			totalTime = Time.now();
			for (i = 0; i < 60; i++)
			{
				window[i] = 0;
			}
		}
	}

	if (currentState == AWAITING_DOOR_STATE)
	{
		if ((Time.now() - totalTime) >= EMAIL_INTERVAL)
		{
			totalTime = Time.now();
			sendEvent();
		}

		int i = 0;
		int sum = 0;

		for (i = 0; i < 59; i++)
		{
			sum += window[i];
			window[i] = window[i+1];
		}
		window[59] = threshold();
		sum += window[59];

		if (sum >= 10) /* Subject to Change */
		{
			totalTime = 0;
			state = AWAITING_START_STATE;
		}
	}
}

byte threshold()
{
	imu.readAccel();

	if (abs(calcMagnitude() - lastMagnitude) >= (restingMagnitudeChange + MOTION_PADDING))
	{
		motionCounter++;
		return 1;
	}
	return 0;
}

float findRestingChange()
{
	int i;
	float totalMagnitudeChange = 0.0F;

	for (i = 0; i < (60000 / LOOP_INTERVAL); i++)
	{
		imu.readAccel();

		if (lastMagnitude != 0.0F)
		{
			lastMagnitude = calcMagnitude();
		}
		else
		{
			float currMagnitude = calcMagnitude();
			totalMagnitudeChange += abs(currMagnitude - lastMagnitude);
			lastMagnitude = currMagnitude;
		}
	}
	return totalMagnitudeChange / (i - 1);
}

float calcMagnitude()
{
	imu.readAccel();
	float x = imu.calcAccel(imu.ax);
	float y = imu.calcAccel(imu.ay);
	float z = imu.calcAccel(imu.az);

	x = x * x;
	y = y * y;
	z = z * z;

	return sqrt(x + y + z) * 1000;
}
