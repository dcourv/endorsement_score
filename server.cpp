#include "server.h"

#include <string>
#include <vector>
// @NOTE should not have to keep iostream
#include <iostream>

using std::string;
using std::vector;

constexpr const char *SERVER_LISTENING_ADDRESS = "http://0.0.0.0:8080";

// @TODO use namespaces

// @NOTE html code could be in another file
const char *HTML_BEGIN = R"foo(<!doctype html>

<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">

  <title>A Basic HTML5 Template</title>
  <meta name="description" content="A simple HTML5 Template for new projects.">
  <meta name="author" content="SitePoint">

  <meta property="og:title" content="A Basic HTML5 Template">
  <meta property="og:type" content="website">
  <meta property="og:url" content="https://www.sitepoint.com/a-basic-html5-template/">
  <meta property="og:description" content="A simple HTML5 Template for new projects.">
  <meta property="og:image" content="image.png">

  <link rel="icon" href="/favicon.ico">
  <link rel="icon" href="/favicon.svg" type="image/svg+xml">
  <link rel="apple-touch-icon" href="/apple-touch-icon.png">

  <link rel="stylesheet" href="css/styles.css?v=1.0">

</head>

<body>
  <!-- your content here... -->
  <script async src="https://platform.twitter.com/widgets.js" charset="utf-8"></script>

)foo";

const char *HTML_END = R"foo(
  <script src="js/scripts.js"></script>
</body>
</html>


<script type="text/javascript">
fetch('http://localhost:8080')
  .then(response => response.json())
  .then(data => console.log(data));
</script>)foo";

const char *TWEET_HTML_BEGIN = R"foo(<blockquote class="twitter-tweet"><a href=")foo";
const char *TWEET_HTML_END = R"foo("></a></blockquote>)foo";

string prepare_page_html(const vector<string> urls) {
	string html(HTML_BEGIN);
	for (const string &url : urls) {
		html.append(TWEET_HTML_BEGIN);
		html.append(url);
		html.append(TWEET_HTML_END);
	}
	html.append(HTML_END);
	return html;
}

void
mongoose_event_handler(
	struct mg_connection *connection,
	int event, void *event_data,
	void *function_data
) {
	// @TODO only output response if propper connection/event type!
	// @NOTE this runs once per second even if no connections

	// @DEBUG (possible/probably unsafe)
	// std::cout << *((char *) event_data) << '\n';
	std::cout << *((int *) function_data) << '\n';


	// @NOTE refer to mongoose examples
	// @NOTE third param is header
	// @TODO add logging!
	// mg_http_reply(connection, 200, "", "my custom reply: %d", -69);
	// @NOTE use %s strings in the future?
	mg_http_reply(
		connection,
		200,
		"",
		prepare_page_html(
			vector<string> {
				"https://twitter.com/alexrossmusic/status/1507827873024909314",
				"https://twitter.com/MitchWagner/status/1457827511497367553"
			}
		).c_str()
	);
}

//@NOTE Handle interrupts, like Ctrl-C
// static int SIGNO;
// static void signal_handler(int signo) {
//   SIGNO = signo;
// }

// @TODO implement
void start_server(/*vector<uint64_t> tweet_ids*/) {
	struct mg_mgr mongoose_event_manager;
	// @TODO make constant?
	// @NOTE level 3 for debug
	mg_log_set("2");

	mg_mgr_init(&mongoose_event_manager);

	// @TEST
	int sixty_nine = 69;

	// @NOTE fourth arg passed to mongoose_event_handler (I believe?)
	if (
		mg_http_listen(
			&mongoose_event_manager,
			SERVER_LISTENING_ADDRESS,
			mongoose_event_handler,
			// &mongoose_event_manager
			// @TEST
			(void *) &sixty_nine
		) == nullptr
	) {
		MG_ERROR(("Cannot listen on %s. Use http://ADDR:PORT or :PORT",
		          SERVER_LISTENING_ADDRESS));
		exit(EXIT_FAILURE);
	}

	MG_INFO(("Mongoose version : v%s", MG_VERSION));
  MG_INFO(("Listening on     : %s", SERVER_LISTENING_ADDRESS));
  // MG_INFO(("Web root         : [%s]", s_root_dir));
  // @TODO make 1000 constant
  // while (s_signo == 0) mg_mgr_poll(&mongoose_event_manager, 1000);
  while (true) {
  	mg_mgr_poll(&mongoose_event_manager, 1000);
  }
  mg_mgr_free(&mongoose_event_manager);
  // MG_INFO(("Exiting on signal %d", s_signo));
  MG_INFO(("Exiting"));
}