#include "math.h"
#include <stdio.h>
#include <stdlib.h>

#define AWAITING_START_STATE 	1
#define WASHING_STATE			2
#define AWAITING_DOOR_STATE		3

#define MAX_EMAIL				5

#define MOTION_PADDING			5

int   	threshold(float iteration);
void 	loop(float iteration);
void	sendEvent();
void	findRestingMagnitudeChange();

int 	currentState;

int 	window[60];

float 	lastMagnitude = 0.0F;
float	restingMagnitudeChange;

int 	notificationCounter = 0;

int main(int argc, char *argv[])
{
	int i;
	currentState = AWAITING_START_STATE;
	restingMagnitudeChange = 0.0F;

	for (i = 0; i < 60; i++)
	{
		window[i] = 0;
	}

	findRestingMagnitudeChange();

	char line[20];
	FILE *data;
	data = fopen("mag.txt", "r");

	lastMagnitude = 998.0F;

	i = 0;
	while (1)
	{
		int temp = currentState;

		if (fgets(line, 20, data) != NULL)
			loop(atof(line));
		else
			break;

		if (temp != currentState)
		{
			if (currentState == AWAITING_START_STATE)
				printf("State Changed - %d: END --> START\n", i);
			else if (currentState == WASHING_STATE)
				printf("State Changed - %d: START --> WASHING\n", i);
			else
				printf("State Changed - %d: WASHING --> END\n", i);
		}
		i++;
	}

	printf("%d - %d\n", currentState, i);
}

void loop(float iteration)
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
		window[59] = threshold(iteration); 
		sum += window[59];

		// printf("SUM: %d\n", sum);

		if (sum >= 30) /* 75% of the last minute detected motion */
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
		window[59] = threshold(iteration);
		sum += window[59];

		if (sum <= 5) /* 83.33% of last minute, no motion detected */
		{ 
			currentState = AWAITING_DOOR_STATE;
			for (i = 0; i < 60; i++)
			{
				window[i] = 0;
			}
		}
	}


	if (currentState == AWAITING_DOOR_STATE)
	{
		if (notificationCounter < 5)
		{
			if (notificationCounter < MAX_EMAIL) {
				sendEvent();
				notificationCounter++;
			}
		}

		int i = 0;
		int sum = 0;

		for (i = 0; i < 59; i++)
		{
			sum += window[i];
			window[i] = window[i+1];
		}
		window[59] = threshold(iteration);
		sum += window[59];

		if (sum >= 5) /* Subject to Change */
		{
			currentState = AWAITING_START_STATE;
			notificationCounter = 0;

			/* reset array to 0 */
			for (i = 0; i < 60; i++)
			{
				window[i] = 0;
			}
		}		
	}
}

int threshold(float iteration)
{
	// printf("%f | %f, %f\n", fabs(iteration - lastMagnitude), iteration, lastMagnitude);
	if (fabsf(iteration - lastMagnitude) >= (restingMagnitudeChange + MOTION_PADDING))
	{
		lastMagnitude = iteration;
		return 1;
	}
	lastMagnitude = iteration;
	return 0;
}

void sendEvent()
{
	printf("EVENT HERE!\n");
}

void findRestingMagnitudeChange()
{
	FILE *background;
	background = fopen("background.txt", "r");

	int i;

	float totalMagnitudeChange = 0.0F;
	float currentMagnitudeChange = 0.0F;

	char line[20];

	for (i = 0; i < 60; i++)
	{
		fgets(line, 20, background);

		if (lastMagnitude == 0.0F)
		{
			lastMagnitude = atof(line);
		}
		else
		{
			float currMagnitude = atof(line);
			currentMagnitudeChange = fabsf(currMagnitude - lastMagnitude);
			totalMagnitudeChange += currentMagnitudeChange;
			lastMagnitude = currMagnitude;
		}
	}
	restingMagnitudeChange = totalMagnitudeChange / (i - 1);

	fclose(background);
}





