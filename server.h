// @NOTE this is a library
#include "mongoose.h"

#include <stdint.h>

void
mongoose_event_handler
(struct mg_connection *connection, int event, void *event_data);

void start_server();