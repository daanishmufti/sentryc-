#include <sentry.h>

void sentry_func(void) {
    sentry_options_t *options = sentry_options_new();
    sentry_options_set_dsn(options, "https://6735680d32229bf117eae898ec453550@o4511568474079232.ingest.us.sentry.io/4511585577992192");
    sentry_options_set_database_path(options, ".sentry-native");
    sentry_options_set_release(options, "my-project-name@2.3.12");
    sentry_options_set_debug(options, 1);
    sentry_init(options);
}