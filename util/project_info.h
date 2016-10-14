#ifndef PROJECT_INFO_H
#define PROJECT_INFO_H

#ifndef APP_VERSION
	#error "No app version specified"
#endif // APP_VERSION



#ifndef TARGET_PRODUCT
	#error "No target product"
#endif // TARGET_PRODUCT



#ifndef TARGET_COMPANY
	#error "No target company"
#endif // TARGET_COMPANY




#define BUILD_FROM __DATE__ ", " __TIME__



#endif // PROJECT_INFO_H
