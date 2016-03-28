#define BEGINNING_STATE  	1
#define WASHING_STATE    	2
#define END_STATE	     	3

#define MOTION_PADDING		10
#define	STILL_PADDING		5

#define MOTION_THRESHOLD	30
#define STILL_THRESHOLD		5

#define BACKGROUND_SAMPLES 	60
#define WINDOW_SIZE			60

#define DELAY_AMOUNT		1000

/*---------------------------------------------------------*/

void 	loop(float data);
int		readData(float data);
int		threshold(float data);

void	getRestingData();
void 	sendEvent();

/*---------------------------------------------------------*/

int 	currentState 	= BEGINNING_STATE;
float	restingChange	= 0.0F;
float	lastMagnitude 	= 0.0F;
int     window[60];	
int		notificationCounter = 0;

char	email[256];
int		maxNotifications;
int 	sleepHourStart;
int		sleepHourEnd;