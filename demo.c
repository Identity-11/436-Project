#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "demo.h"

int main(int argc, char *argv[])
{
	int i = 0;

	getRestingData();

	char line[20];
	FILE *data;
	data = fopen("mag.txt", "r");

	int tempState;

	/* PlaceHolder Preferences */
	strncpy(email, "cs436project@gmail.com", 256);
	maxNotifications 	= 5;
	sleepHourStart		= 20;
	sleepHourEnd 		= 6;


	printf("\n\t----------------------------------------\n\n");

	while (1)
	{
		tempState = currentState;

		if (fgets(line, 20, data) != NULL)
			loop(atof(line));
		else
			break;


		if (tempState != currentState)
		{
			printf("\tState Changed: %d: ", i);
			if (currentState == WASHING_STATE)
				printf("Waiting State --> Washing State\n");
			else if (currentState == END_STATE)
				printf("Washing State --> End State\n");
			else if (currentState == BEGINNING_STATE)
				printf("End State --> Waiting State\n");
		}
		i++;
	}

	printf("\n\t----------------------------------------\n\n");

	printf("\tData Stream Ended: %d values | Current State: ", i);
	if (currentState == BEGINNING_STATE)
		printf("Waiting State\n");
	else if (currentState == WASHING_STATE)
		printf("Washing State\n");
	else if (currentState == END_STATE)
		printf("End State\n");

	printf("\n\t----------------------------------------\n\n");
}


/******************************************************************
 *
 *	loop():
 *		The main loop. Checks motion data, changes state if needed.
 *		Also sends e-mails!
 *
 ******************************************************************/
void loop(float data)
{	
	if (currentState == BEGINNING_STATE)
	{
		/* If we've passed the threshold, go to next state, clear window */
		if (readData(data) >= MOTION_THRESHOLD)
		{
			currentState = WASHING_STATE;
			for (int i = 0; i < WINDOW_SIZE; i++)
				window[i] = 1;
		}
	}
	else if (currentState == WASHING_STATE)
	{
		if (readData(data) <= STILL_THRESHOLD)
		{
			currentState = END_STATE;
			for (int i = 0; i < WINDOW_SIZE; i++)
				window[i] = 0;
		}
	}
	else if (currentState == END_STATE)
	{
		if (readData(data) >= STILL_THRESHOLD)
		{
			currentState = BEGINNING_STATE;
			notificationCounter = 0;

			for (int i = 0; i < WINDOW_SIZE; i++)
				window[i] = 0;
		}
		else if (notificationCounter < maxNotifications)
		{
			sendEvent();
			sleep(1);
		}
	}
}

/******************************************************************
 *
 *	readData():
 *		Iterates over the array summing up the past 60 motion events
 *		shifting the values to the left, then adding in the current
 *		reading to the end. Then returns the sum of the events.
 *
 ******************************************************************/
int readData(float data)
{
	int i;
	int sum = 0;
	for (i = 0; i < WINDOW_SIZE - 1; i++)
	{
		sum += window[i];
		window[i] = window[i+1];
	}
	window[WINDOW_SIZE - 1] = threshold(data);
	sum += window[WINDOW_SIZE - 1];

	return sum;
}

/******************************************************************
 *
 *	threshold():
 *		Depending on current state, calculates whether motion has 
 *		been detect (Return 1), or if it has not (Return 0)
 *
 ******************************************************************/
int threshold(float data)
{
	/* Different Paddings Depending on State */
	if (currentState == BEGINNING_STATE)
	{
		if (fabsf(data - lastMagnitude) >= (restingChange + MOTION_PADDING))
		{
			lastMagnitude = data;
			return 1;
		}
	}
	else
	{
		if (fabsf(data - lastMagnitude) >= (restingChange + STILL_PADDING))
		{
			lastMagnitude = data;
			return 1;
		}
	}
	lastMagnitude = data;
	return 0;
}

/******************************************************************
 *
 * getRestingData():
 *		Reads from a file to calculate the average difference
 *		between resting values
 *
 ******************************************************************/
void getRestingData()
{
	int 	i 				= 0;
	float 	totalChange 	= 0.0F;
	float	currentChange 	= 0.0F;
	char	line[20];
	FILE 	*background;
	
	background = fopen("background.txt", "r");

	for (i = 0; i < BACKGROUND_SAMPLES; i++)
	{
		fgets(line, 20, background);

		if (lastMagnitude == 0.0F)
		{
			lastMagnitude = atof(line);
		}
		else
		{
			float tempMagnitude = atof(line);
			currentChange = fabsf(tempMagnitude - lastMagnitude);
			totalChange += currentChange;
			lastMagnitude = tempMagnitude;
		}
	}
	restingChange = totalChange / (i - 1);
	fclose(background);
}

void sendEvent()
{
	time_t epoch = time(NULL);
	struct tm *hours;
	hours = localtime(&epoch);

	if ((hours->tm_hour < sleepHourStart) && (hours->tm_hour > sleepHourEnd))
	{
		printf("\t\tEVENT: Sent Email to %s\n", email);
		notificationCounter++;
		// system("mail -s \"Washing Machine Finished\" cs436project@gmail.com < empty.txt");
	}
}
