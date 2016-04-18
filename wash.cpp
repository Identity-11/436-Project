
#include "HttpClient/HttpClient.h"
#include "SparkFunLSM9DS1/SparkFunLSM9DS1.h"

#include "math.h"

#define BEGINNING_STATE     1
#define WASHING_STATE       2
#define END_STATE           3

#define MOTION_PADDING      7
#define STILL_PADDING       5

#define MOTION_THRESHOLD    60 /* 50% of 120 samples */
#define STILL_THRESHOLD     10  /* 8.33% of 120 Samples */
#define DOOR_THRESHOLD      3   /* 3.33% of 120 samples */

#define BACKGROUND_SAMPLES  120
#define WINDOW_SIZE         120

#define LOOP_INTERVAL       500

#define TIME_ZONE           -7 /* UTC for AZ currently */

/*------------------------------------------------------------*/

LSM9DS1 imu;
http_request_t request;
http_response_t response;

HttpClient http;
time_t  currentTime;

http_header_t headers[] = {
    { "Accept" , "*/*"},
    { NULL, NULL }
};

int     currentState;
float   restingChange;
float   lastMagnitude;
int     window[WINDOW_SIZE];
int     notificationCounter;

char    email[256];
int     maxNotifications;
int     sleepHourStart;
int     sleepHourEnd;

int     timeoutCounter;
int     emailCounter;

/*------------------------------------------------------------*/

float getData();
float getRestingData();
float calcMagnitude();
int   threshold();
void  sendEvent();
int   readData();

/***************************************************************************************
 *
 *  setup():
 *      Initializes the device and sets up IMU addresses, then initializes the IMU. 
 *      Sets up some default state values then retrieves the resting data. After the
 *      resting data has been created, enters an infinite loop that calls the main
 *      loop function and delays for the loop interval (1 second default)
 *
 *  Parameters:
 *      None
 *
 *  Side Effects:
 *      IMU will be accessed
 *
 *  Return:
 *      Nothing
 *
 **************************************************************************************/
void setup()
{
    Serial.begin(115200);

    imu.settings.device.commInterface   = IMU_MODE_I2C;
    imu.settings.device.agAddress       = 0x6B;
    imu.settings.device.mAddress        = 0x1E;

    imu.begin();
    
    request.hostname    = "cs436project.comxa.com";
    request.port        = 80;
    request.path        = "/devices/20001c000747343337373738.php";

    currentState        = BEGINNING_STATE;
    lastMagnitude       = 0.0F;

    restingChange       = getRestingData();
    
    Serial.print("Done with Resting Data\n");
    Serial.print("Average Change: ");
    Serial.print(restingChange);
    Serial.print("\n");

    /* Clear Window */
    for (int i = 0; i < WINDOW_SIZE; i++)
        window[i] = 0;

    while (1)
    {
        loop();
        delay(LOOP_INTERVAL);
    }
}

/***************************************************************************************
 *
 *  loop():
 *      Main program loop. Checks current state and conditions to move onto next state.
 *      If conditions have been met, moves on to next state. 
 *
 *          * In Beginning state, checks to see if motion thresholds have been passed, if
 *              so, sets all window values to 1 (100% motion) and continues to washing 
 *              state
 *
 *          * In Washing state, waits for 15 minutes (900 loops at 1s/loop), then checks
 *              to see if still thresholds have been met, then sets all window values to
 *              to 0 (0% motion) and goes to end state
 *
 *          * In End state, checks if a minor amount of motion has been met in the last
 *              minute. If so, considers that the door opening, sets Window values to 0
 *              (0% motion) and returns to beginning state. If the motion has not been 
 *              checks if it's time to send an e-mail (every 15 minutes), and if so
 *              calls the sendEvent() function.s
 *
 *  Parameters:
 *      None
 *
 *  Side Effects:
 *      E-mail may be sent, accelerometer will be accessed
 *
 *  Returns:
 *      Nothing
 *
 **************************************************************************************/
void loop()
{
    if (currentState == BEGINNING_STATE)
    {
        if (readData() >= MOTION_THRESHOLD)
        {
            currentState    = WASHING_STATE;
            timeoutCounter  = 0;
            Serial.print("Entering Washing State\n");

            for (int i = 0; i < WINDOW_SIZE; i++)
                window[i] = 1;

        }
    }
    else if (currentState == WASHING_STATE)
    {
        if (timeoutCounter >= 1800) /* 15 minutes at the current loop interval 500ms */
        {
            if (readData() <= STILL_THRESHOLD)
            {
                currentState    = END_STATE;
                timeoutCounter  = 0;
                Serial.print("Entering Ending State\n");
                
                /* Sends first E-Mail */
                sendEvent();
                notificationCounter = 1;
                    
                for (int i = 0; i < WINDOW_SIZE; i++)
                    window[i] = 0;
            }
        }
        else
        {
            readData();
            timeoutCounter++;
        }
    }
    else if (currentState == END_STATE)
    {
        if (readData() >= (DOOR_THRESHOLD))
        {
            currentState = BEGINNING_STATE;
            notificationCounter = 0;
            Serial.print("Door Opened!\n");

            for (int i = 0; i < WINDOW_SIZE; i++)
                window[i] = 0;
        }
        else if (emailCounter >= 1800) /* 15 minutes at current interval of 500ms */
        {
            emailCounter = 0;

            if (notificationCounter < maxNotifications)
                sendEvent();
        }
        else
        {
            emailCounter++;
        }
    }
}

/***************************************************************************************
 * 
 *  readData():
 *      Iterates over the window (last 60 parses) and determines the sum of all the
 *      positions that indicated the threshold has been passed (if they are 1). Also
 *      shifts all positions 1 to the left and then adds in the current threshold check
 *      to the end of the window.
 *
 *  Parameters:
 *      None
 *
 *  Side Effects:
 *      Accelerometer is accessed (threshold is called)
 *
 *  Return:
 *      Sum of all the values in the window that indicate motion threshold passed
 *
 **************************************************************************************/
int readData()
{
    int i;
    int sum = 0;
    for (i = 0; i < WINDOW_SIZE - 1; i++)
    {
        sum += window[i];
        window[i] = window[i+1];
    }

    window[WINDOW_SIZE - 1] = threshold();
    sum += window[WINDOW_SIZE - 1];

    return sum;
}

/***************************************************************************************
 *
 *  getRestingData():
 *      Over the course of a minute, calculates the magnitude every loop interval, then
 *      generates the average of the minute. This is used to find the background data
 *
 *  Parameters:
 *      None
 *
 *  Side Effects:
 *      Accelerometer is Accessed
 *
 *  Returns:
 *      Float representing the average change in motion over the minute (in g's
 *      multiplied by 1000)
 *
 **************************************************************************************/
float getRestingData()
{
    int     i;
    float   totalChange         = 0.0F;
    float   currentMagnitude    = 0.0F;

    if (lastMagnitude == 0.0F)
    {
        lastMagnitude = calcMagnitude();
    }

    /* Loop for one minute */
    for (i = 0; i < (60000 / LOOP_INTERVAL); i++)
    {
        float currentMagnitude = calcMagnitude();
        
        Serial.println(currentMagnitude);
        
        /* Add up the total change taken over the minute */
        totalChange += fabsf(currentMagnitude - lastMagnitude);
        lastMagnitude = currentMagnitude;
        delay(LOOP_INTERVAL);
    }

    return (totalChange / i);
}

/***************************************************************************************
 * 
 *  calcMagnitude():
 *      Accesses the accelerometer, calculates the force being exerted on the x/y/z
 *      planes, then calculates the magnitude of all the forces and multiplies by 
 *      1000 for easier use.
 *
 *  Parameters:
 *      None
 *
 *  Side Effects:
 *      Accelerometer is accessed
 *
 *  Returns:
 *      Float representing magnitude of current forces
 *
 **************************************************************************************/
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

/***************************************************************************************
 *
 *  threshold():
 *      Calculates the current magnitude on the accelerometer, then compares it against
 *      the resting thresholds. Returns 1 if the motion passed the threshold.
 *
 *  Parameters:
 *      None
 *
 *  Side Effects:
 *      Accelerometer is accessed
 *
 *  Return:
 *      1 if threshold passed, 0 otherwise
 *
 **************************************************************************************/
int threshold()
{
    float currentMagnitude = calcMagnitude();
    Serial.print(currentMagnitude);
    Serial.print("\n");
    
    /* If in Beginning state, use the motion Padding, otherwise use the still padding */
    if (currentState == BEGINNING_STATE)
    {
        if (fabsf(currentMagnitude - lastMagnitude) >= (restingChange + MOTION_PADDING))
        {
            lastMagnitude = currentMagnitude;
            return 1;
        }
    }
    else
    {
        if (fabsf(currentMagnitude - lastMagnitude) >= (restingChange + STILL_PADDING))
        {
            lastMagnitude = currentMagnitude;
            return 1;
        }
    }
    lastMagnitude = currentMagnitude;
    return 0;
}

/***************************************************************************************
 *
 *  sendEvent():
 *      Retrieves contact information from server, then checks time. If the the time is
 *      in the allowed range, sends a notification to the e-mail address specified by
 *      the server. If server is down, checks current information, and if valid, sends
 *      notification.
 *
 *
 *  Parameters:
 *      None
 *
 *  Side Effects:
 *      HTTP GET request sent, possibility of e-mail being sent
 *
 *  Returns:
 *      Nothing
 *
 ***************************************************************************************/
void sendEvent()
{
    char body[512];    
    
    Serial.println("Attempting to send Event...");
    
    Time.zone(TIME_ZONE); /* Arizona Time, Change This for wherever you want */
    currentTime = Time.hour();

    http.get(request, response, headers);
    if ((response.status >= 300) || (response.status < 200))
    {
        /* If Website down, but e-mail still not null, send mail to old address */
        Serial.println("Could Not Contact Website!");
        if (email[0] != 0)
        {
            if ((currentTime < sleepHourStart) && (currentTime >= sleepHourEnd))
            {
                notificationCounter++;
                Particle.publish("Email", email, 60, PRIVATE); 
            }
        }
    }
    else
    {
        Serial.println("Website Found!");
        
        strncpy(body, response.body, 512);
        char *line = NULL;
        
        line = strtok(body, "\n");
        strncpy(email, line, strlen(line));
        
        line = strtok(NULL, "\n");
        maxNotifications = atoi(line);
        
        line = strtok(NULL, "\n");
        sleepHourEnd = atoi(line);
        
        line = strtok(NULL, "\n");
        sleepHourStart = atoi(line);
        
        if ((currentTime < sleepHourStart) && (currentTime >= sleepHourEnd))
        {
            notificationCounter++;
            Particle.publish("Email", email, 60, PRIVATE); 
        }
    }
}
