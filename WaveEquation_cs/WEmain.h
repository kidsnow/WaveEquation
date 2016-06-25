#define raindrop

#ifdef raindrop
#define GRIDSIDENUM 64
#define GRIDLENGTH 100.0
#define INITSPEED 30.0
#define ALPHASQUARE 7.0
#define TIMEINTERVAL 0.0006
#define SIGMA 1.0
#define PI 3.1415926535
#define EEE 2.71828182
#define WAITFOR 0
#define RAINFALL

#endif

#ifdef wave
#define GRIDSIDENUM 64
#define GRIDLENGTH 100.0
#define INITSPEED 1200.0
#define ALPHASQUARE 7.0
#define TIMEINTERVAL 0.0006
#define SIGMA 5.0
#define PI 3.1415926535
#define EEE 2.71828182
#define WAITFOR 3000

struct Grid{
	float x, y, z, w;
};
#endif

////점성이 약한 매질
//#define GRIDSIDENUM 64
//#define GRIDLENGTH 100.0
//#define INITSPEED 900.0
//#define ALPHASQUARE 2.0
//#define TIMEINTERVAL 0.0006
//#define SIGMA 3.0
//#define PI 3.1415926535
//#define EEE 2.71828182
//#define WAITFOR 3000
//
//struct Grid{
//	float x, y, z, w;
//};

////점성이 강한 매질
//#define GRIDSIDENUM 64
//#define GRIDLENGTH 100.0
//#define INITSPEED 900.0
//#define ALPHASQUARE 9.0
//#define TIMEINTERVAL 0.0006
//#define SIGMA 3.0
//#define PI 3.1415926535
//#define EEE 2.71828182
//#define WAITFOR 3000
//
//struct Grid{
//	float x, y, z, w;
//};