#include <android/log.h>
#include <android_native_app_glue.h>

#define TAG "bob-test"

void android_main(struct android_app* app) {
	__android_log_print(ANDROID_LOG_INFO, TAG, "Hello from Bob!");

	while (1) {
		int events;
		struct android_poll_source* source;

		while (ALooper_pollOnce(0, NULL, &events, (void**) &source) >= 0) {
			if (source != NULL) {
				source->process(app, source);
			}

			if (app->destroyRequested) {
				return;
			}
		}
	}
}
