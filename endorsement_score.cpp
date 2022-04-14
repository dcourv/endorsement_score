#include <cmath>
#include <vector>
#include <algorithm>
#include <iostream>
// #include <locale>
#include <unordered_map>
#include <fstream>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <liboauthcpp/liboauthcpp.h>

using std::string;

using std::cout;
using std::cerr;
using std::endl;

using std::vector;
using std::unordered_map;

using json = nlohmann::json;

enum class Backend { TWITTER, MASTODON };

#define BOLD_ON "\e[1m"
#define BOLD_OFF "\e[0m"

#define TWITTER_MAX_LIKING_USERS_PAGES 10
// @NOTE must be less than 200
#define TWITTER_MAX_FETCHED_TWEETS 50
#define TWITTER_LIKING_USERS_RATE_LIMIT 75

static string TWITTER_BEARER_TOKEN;
static const char *TWITTER_BEARER_TOKEN_URI = "twitter_bearer_token.txt";

static string TWITTER_CONSUMER_KEY;
static const char *TWITTER_CONSUMER_KEY_URI = "twitter_api_key.txt";

static string TWITTER_CONSUMER_SECRET;
static const char *TWITTER_CONSUMER_SECRET_URI = "twitter_api_secret.txt";

// @TODO does this need to be global?
static int API_CALLS_CONSUMED = 0;

double rms(vector<long long> array);

string
produce_oauth_protected_resource_url
(string oauth_access_token, string oauth_access_token_secret);

// @TODO add both tweet engagement and retweet engagement

// @TODO generalize
// tweet/toot (mastodon status) info
typedef struct {
	string text;
	string id;
	string url;
	string author_id;
	string author_display_name;
	string author_username;

	long long share_count;
	long long reply_count;
	long long like_count;
	long long quote_count;

	double endorsement_score;
	double composite_score;

	bool is_retweet;
	string retweeted_tweet_id;

	bool was_reached_before_rate_limit;
} TweetInfo;

nlohmann::basic_json<>
http_single_request_to_json(const string url, const string bearer_token = "") {
	// @DEBUG
	// cout << "sending request to " << url << endl;
	cpr::Response r =
		cpr::Get(
			cpr::Url{url},
			cpr::Header{{"Authorization", "Bearer " + bearer_token}}
		);

	if(r.status_code == 0) {
		cerr << r.error.message << endl;
	} else if (r.status_code >= 400) {
		cerr << "Error [" << r.status_code << "] making request" << endl;
		cerr << "Url: " << url << endl;
		cerr << r.text << endl;

		// @TODO better error handling/message
		throw std::runtime_error("Failed http request");
	}
#if 0
	else {
		// @LOG
		cout << "Request took " << r.elapsed << endl;
		cout << "Body:" << endl << r.text;
	}
#endif

	++API_CALLS_CONSUMED;

	if (r.text == "") {
		cerr << "Error: empty body text from " << url << endl;
	}
	
	auto json_object = json::parse(r.text);
	// @DEBUG
	// cout << json_object.dump(2) << endl;
	return json_object;
}

// @TODO make these params, not globals!
string request_token_url = "https://api.twitter.com/oauth/request_token";
string request_token_query_args = "oauth_callback=oob";
string authorize_url = "https://api.twitter.com/oauth/authorize";
string access_token_url = "https://api.twitter.com/oauth/access_token";

string get_user_string(string prompt) {
	cout << prompt << " ";

	string res;
	std::cin >> res;
	cout << endl;;
	return res;
}

// @TODO error handling?
void write_string_to_file(string filename, string data) {
	std::ofstream output_filestream(filename);
	output_filestream << data;
	output_filestream.close();
}

// @TODO handle nonexistant file?
string read_file_to_string(string filename) {
	std::ifstream input_file_stream(filename);
	std::stringstream string_stream;
	string_stream << input_file_stream.rdbuf();
	return string_stream.str();
}

// @TODO make globals function arguments
string oauth_make_protected_resource_url(void) {
	// @TODO security audit
	// @NOTE do this in main?
	// @TODO use config file
	string oauth_token_key =
		read_file_to_string("twitter_oauth_access_token_key.txt");
	string oauth_token_secret =
		read_file_to_string("twitter_oauth_access_token_secret.txt");

	// @LOG
	// cout << "oauth_token_key: " << oauth_token_key << endl;
	// cout << "oauth_token_secret: " << oauth_token_secret << endl;

	if (oauth_token_key.empty() || oauth_token_secret.empty()) {
		// @LOG
		cout << "Could not find local oauth key, signing you into twitter..."
		     << endl;

		// @TODO generalize?
		OAuth::Consumer consumer(TWITTER_CONSUMER_KEY, TWITTER_CONSUMER_SECRET);
		OAuth::Client oauth(&consumer);

		// Step 1: Get a request token. This is a temporary token that is used for
		// having the user authorize an access token and to sign the request to
		// obtain said access token.
		string base_request_token_url =
			request_token_url + (request_token_query_args.empty() ?
				string("") :
				(string("?") + request_token_query_args));
		string oAuthQueryString =
			oauth.getURLQueryString(OAuth::Http::Get, base_request_token_url);

		// @TODO error handling
		string request_token_url_oauth = request_token_url + "?" + oAuthQueryString;
		string request_token_response = cpr::Get(cpr::Url(request_token_url_oauth)).text;

		// @LOG
		// cout << "url: " << request_token_url << endl;
		// cout << "response text: " << request_token_response << endl;

		OAuth::Token request_token = OAuth::Token::extract(request_token_response);

		// Step 2: Redirect to the provider. Since this is a CLI script we
		// do not redirect. In a web application you would redirect the
		// user to the URL below.
		cout << "Go to the following link in your browser to authorize this "
			"application on a user's account:" << endl;;
		cout << authorize_url << "?oauth_token=" << request_token.key() << endl;

		// After the user has granted access to you, the consumer, the
		// provider will redirect you to whatever URL you have told them
		// to redirect to. You can usually define this in the
		// oauth_callback argument as well.
		string pin = get_user_string("What is the PIN?");
		request_token.setPin(pin);

		// Step 3: Once the consumer has redirected the user back to the
		// oauth_callback URL you can request the access token the user
		// has approved. You use the request token to sign this
		// request. After this is done you throw away the request token
		// and use the access token returned. You should store the oauth
		// token and token secret somewhere safe, like a database, for
		// future use.
		oauth = OAuth::Client(&consumer, &request_token);
		// Note that we explicitly specify an empty body here (it's a GET) so we can
		// also specify to include the oauth_verifier parameter
		oAuthQueryString =
			oauth.getURLQueryString(
				OAuth::Http::Get, access_token_url, string(""), true);
		cout << "Enter the following in your browser to get the final access token"
		     << " & secret: " << endl;
		cout << access_token_url << "?" << oAuthQueryString;
		cout << endl;;

		// @TODO rename
		auto access_token_url_oauth = access_token_url + "?" + oAuthQueryString;
		auto access_token_response = cpr::Get(cpr::Url(access_token_url_oauth)).text;

		// Once they've come back from the browser, extract the token and
		// token_secret from the response
		// string access_token_resp = get_user_string("Enter the response:");

		// On this extractToken, we do the parsing ourselves (via the library) so we
		// can extract additional keys that are sent back, in the case of twitter,
		// the screen_name
		OAuth::KeyValuePairs access_token_response_data =
			OAuth::ParseKeyValuePairs(access_token_response);
		OAuth::Token access_token =
			OAuth::Token::extract(access_token_response_data);

		// @LOG
		// cout << "Access token:" << endl;;
		// cout << "    - oauth_token        = " << access_token.key() << endl;
		// cout << "    - oauth_token_secret = " << access_token.secret() << endl;
		// cout << endl;;
		// cout << "You may now access protected resources using the access tokens "
		//      << "above." << endl;;
		// cout << endl;

		std::pair<OAuth::KeyValuePairs::iterator, OAuth::KeyValuePairs::iterator>
		screen_name_its
			= access_token_response_data.equal_range("screen_name");
		// @LOG
		// for (OAuth::KeyValuePairs::iterator it = screen_name_its.first;
		//      it != screen_name_its.second;
		//      ++it) {
		// 	cout << "Using OAuth authentication for user: "
		// 	     << it->second << endl;
		// }

		oauth_token_key = access_token.key();
		oauth_token_secret = access_token.secret();

		// @TODO security audit
		// @TODO store filenames as constants
		write_string_to_file(
			"twitter_oauth_access_token_key.txt",
			oauth_token_key);
		write_string_to_file(
			"twitter_oauth_access_token_secret.txt",
			oauth_token_secret);
	}

	string url =
		produce_oauth_protected_resource_url(oauth_token_key, oauth_token_secret);

	return url;
}

string
produce_oauth_protected_resource_url
(string oauth_token, string oauth_token_secret) {
	// @TODO make this a parameter
	string oauth_protected_resource =
		"https://api.twitter.com/1.1/statuses/home_timeline.json";
	string oauth_protected_resource_params = "count=200";

	// We assume you have gotten the access token. You may have e.g., used
	// simple_auth to get it.

	OAuth::Consumer consumer(TWITTER_CONSUMER_KEY, TWITTER_CONSUMER_SECRET);
	OAuth::Token token(oauth_token, oauth_token_secret);
	OAuth::Client oauth(&consumer, &token);

	// Get the query string. Note that we pass in the URL as if we were
	// accessing the resource, but *the output query string includes the
	// parameters you passed in*. Below, we append the result only to the base
	// URL, not the entire URL we passed in here.
	string oAuthQueryString =
		oauth.getURLQueryString(
			OAuth::Http::Get,
			oauth_protected_resource + "?" + oauth_protected_resource_params);

	return oauth_protected_resource + "?" + oAuthQueryString;
}

vector<long long>
mastodon_status_id_to_followers_list(const string status_id) {
	string favoriting_users_url = "https://mastodon.social/api/v1/statuses/"
		+ status_id +	"/favourited_by";

	auto json_response = http_single_request_to_json(favoriting_users_url);

	vector<long long> follower_counts;

	for (auto user : json_response) {
		follower_counts.push_back(
			user["followers_count"].get<long long>()
		);
	}

	return follower_counts;
}

// @TODO name page numbers constant?
// @TODO return api calls consumed!
vector<long long>
tweet_id_to_liking_followers_list(
	const string tweet_id,
	int number_of_pages = TWITTER_MAX_LIKING_USERS_PAGES
) {
	const string base_request_url =
		"https://api.twitter.com/2/tweets/" + tweet_id + "/liking_users"
			"?user.fields=public_metrics";

	vector<long long> follower_counts;
	string pagination_token = "";
	for (int page_number = 0; page_number < number_of_pages; ++page_number) {
		string request_url = base_request_url;
		if (pagination_token != "") {
			request_url += "&pagination_token=" + pagination_token;
		}

		auto liking_users_json =
			http_single_request_to_json(request_url, TWITTER_BEARER_TOKEN);
		
		int result_count = liking_users_json["meta"]["result_count"].get<int>();
		if (result_count == 0) {
			return follower_counts;
		}
		pagination_token = liking_users_json["meta"]["next_token"];

		auto liking_users_json_data = liking_users_json["data"];
		for (int i = 0; i < liking_users_json_data.size(); ++i) {
		auto user_json = liking_users_json_data[i];
			// @TODO catch parse error?
			long long follower_count = 0;
			try {
				follower_count =
					user_json["public_metrics"]["followers_count"].get<long long>();
			} catch (json::exception &e) {
				cout << "Error parsing follower counts: " << e.what() << endl;
				// @DEBUG
				cout << "JSON object: " << user_json << endl;
				throw e;
			}
			// @LOG
			// if (follower_count > 10'000) {
			// 	cout << "highly followed user: "
			// 	     << '@' << user_json["username"].get<string>()
			// 	     << " on tweet: " << tweet_id
			// 	     << " with " << follower_count << " followers"
			// 	     << endl;
			// }
			follower_counts.push_back(follower_count);
		}
	}

	return follower_counts;
}

vector<TweetInfo> tweet_ids_to_tweet_info_list(vector<string> tweet_ids) {

	string request_url = "https://api.twitter.com/2/tweets?ids=";
	for (int i = 0; i < tweet_ids.size() - 1; ++i) {
		request_url += tweet_ids[i];
		request_url += ",";
	}
	request_url += tweet_ids.back();
	request_url +=
		"&tweet.fields=public_metrics,referenced_tweets&expansions=author_id,"
		"referenced_tweets.id";

	auto response_json =
		http_single_request_to_json(request_url, TWITTER_BEARER_TOKEN);
	// @DEBUG
	// cout << response_json << endl;

	vector<TweetInfo> display_info_list {};
	// @TODO try catch?
	auto response_json_data = response_json["data"];

	auto includes_json = response_json["includes"];

	unordered_map<string, int> author_id_to_include_index;
	for (int i = 0; i < includes_json["users"].size(); ++i) {
		// @TODO try catch?
		auto user_json = includes_json["users"][i];
		string user_id = user_json["id"].get<string>();
		author_id_to_include_index[user_id] = i;
	}

	unordered_map<string, long long> included_tweet_id_to_likes;

	for (auto included_tweet_json : includes_json["tweets"]) {
		// @TODO try catch?
		string id = included_tweet_json["id"].get<string>();
		long long likes
			= included_tweet_json["public_metrics"]["like_count"].get<long long>();
		included_tweet_id_to_likes[id] = likes;
	}

	// @LOG
	// for (auto map_it : included_tweet_id_to_likes) {
	// 	cout << map_it.first << ": " << map_it.second << endl;
	// }

	for (int i = 0; i < response_json_data.size(); ++i) {
		TweetInfo info {};
		try {
			auto tweet_json = response_json_data[i];
			info.id = tweet_json["id"].get<string>();

			assert(info.id == tweet_ids[i]);

			info.text = tweet_json["text"].get<string>();
			info.url = "https://twitter.com/i/web/status/" + tweet_ids[i];
			
			info.author_id = tweet_json["author_id"].get<string>();

			int author_include_index = author_id_to_include_index.at(info.author_id);
			auto author_json
				= response_json["includes"]["users"][author_include_index];
			info.author_display_name = author_json["name"];
			info.author_username = author_json["username"];
			
			auto public_metrics_json = tweet_json["public_metrics"];
			info.share_count = public_metrics_json["retweet_count"].get<long long>();
			info.reply_count = public_metrics_json["reply_count"].get<long long>();

			auto referenced_tweets_json = tweet_json["referenced_tweets"];
			for (auto referenced_tweet_json : referenced_tweets_json) {
				if (referenced_tweet_json["type"].get<string>() == "retweeted") {
					info.is_retweet = true;
					info.retweeted_tweet_id = referenced_tweet_json["id"].get<string>();
				}
				// @LOG
				// cout << "is rewteet: " << info.is_retweet << endl;
				// cout << "retweeted_tweet_id: " << info.retweeted_tweet_id << endl;
			}

			// @NOTE if the tweet is a retweet, likes (and possibly other public
			// metrics?) will be zero; use data from retweeted tweet
			info.like_count =
				info.is_retweet ?
					included_tweet_id_to_likes[info.retweeted_tweet_id] :
					public_metrics_json["like_count"].get<long long>();
			
			// @NOTE does same apply here?
			info.quote_count = public_metrics_json["quote_count"].get<long long>(); 

			// @LOG
			cout << "Calculating score for tweet " << i << "..." << endl;

			// @LOG
			cout << "API calls consumed: " << API_CALLS_CONSUMED << endl;

			vector<long long> followers_list;
			if (API_CALLS_CONSUMED + TWITTER_MAX_LIKING_USERS_PAGES >
					TWITTER_LIKING_USERS_RATE_LIMIT) {
				followers_list = vector<long long> {};
				info.was_reached_before_rate_limit = false;
			}
			else {
				// @TEST
				// @TODO name constants?
				int pages_to_request =
					std::min(
						info.like_count / 100 / 10,
						(long long) TWITTER_MAX_LIKING_USERS_PAGES
					);

				if (pages_to_request == 0) {
					pages_to_request = 1;
				}

				// @LOG
				cout << "Requesting " << pages_to_request << " pages of likes from"
					" a tweet with " << info.like_count << " likes." << endl;

				if (info.is_retweet) {
					// @TODO concat followers list of both tweet and retweet?
					followers_list =
						tweet_id_to_liking_followers_list(
							info.retweeted_tweet_id,
							pages_to_request
						);
				}
				else {
					followers_list =
						tweet_id_to_liking_followers_list(info.id, pages_to_request);
				}
				info.was_reached_before_rate_limit = true;
			}

			info.endorsement_score = rms(followers_list);

		}
		catch (json::exception &e) {
			// @DEBUG
			cout << "Error parsing Tweet info: " << e.what() << endl;
			cout << "JSON object: " << response_json << endl;
		}
		
		display_info_list.push_back(info);
	}

	return display_info_list;
}

int main(int argc, char const *argv[]) {

	// @NOTE for decimal separators
	// cout.imbue(std::locale(""));

	// @TODO update with feed option
	string help_message =
		BOLD_ON "Usage: " BOLD_OFF
		"./endorsement_score [-t [id ...]] [-m id ...]\n"
		"-t : fetch fifty tweets from your timeline and display them ranked "
		"by endorsement score\n"
		"-t id ... : calculate the endorsement scores for tweets with given id[s]"
		"\n"
		"-m id ... : calculate the endorsement scores for Mastodon statuses with"
			" given id[s]\n"
		"You must pass either -t or -m, but not both";

	if (argc < 2) {
		std::cerr << help_message << endl;
		return 1;
	}

	Backend backend;

	if (string { argv[1] }.compare("-t") == 0) {
		backend = Backend::TWITTER;
	} else if (string { argv[1] }.compare("-m") == 0) {
		backend = Backend::MASTODON;
	} else {
		std::cerr << help_message << endl;
		return 1;
	}

	TWITTER_CONSUMER_KEY = read_file_to_string(TWITTER_CONSUMER_KEY_URI);
	if (TWITTER_CONSUMER_KEY.empty()) {
		TWITTER_CONSUMER_KEY = get_user_string("Enter twitter api key:");
		write_string_to_file(TWITTER_CONSUMER_KEY_URI, TWITTER_CONSUMER_KEY);
	}

	TWITTER_CONSUMER_SECRET = read_file_to_string(TWITTER_CONSUMER_SECRET_URI);
	if (TWITTER_CONSUMER_SECRET.empty()) {
		TWITTER_CONSUMER_SECRET = get_user_string("Enter twitter api key secret:");
		write_string_to_file(TWITTER_CONSUMER_SECRET_URI, TWITTER_CONSUMER_SECRET);
	}

	TWITTER_BEARER_TOKEN = read_file_to_string(TWITTER_BEARER_TOKEN_URI);
	if (TWITTER_BEARER_TOKEN.empty()) {
		TWITTER_BEARER_TOKEN = get_user_string("Enter twitter bearer token:");
		write_string_to_file(TWITTER_BEARER_TOKEN_URI, TWITTER_BEARER_TOKEN);
	}

	// @TODO clean up?

	vector<string> ids;

	if (argc == 2) {
		string url = oauth_make_protected_resource_url();
		// @LOG
		// cout << "url: " << endl;

		auto response_json = http_single_request_to_json(url);
		vector<string> tweet_ids;
		for (auto tweet_json : response_json) {
			// can also use "id_str" with string?
			long long id_long_long = tweet_json["id"].get<long long>();
			tweet_ids.push_back(std::to_string(id_long_long));
		}

		// @LOG
		// cout << "tweet ids: " << endl;
		// for (string tweet_id : tweet_ids) {
		// 	cout << tweet_id << endl;
		// }

		for (int i = tweet_ids.size() - TWITTER_MAX_FETCHED_TWEETS;
		     i < tweet_ids.size();
		     ++i)
		{
			ids.push_back(tweet_ids[i]);
		}
	}
	else {
		// @TODO validate input?
		for (int i = 2; i < argc; ++i) {
			ids.push_back(argv[i]);
		}
	}

	if (backend == Backend::MASTODON) {
		// @TODO refactor into generalized block
		vector<vector<long long>> mastodon_follower_lists (ids.size());
		for (int i = 0; i < ids.size(); ++i) {
			mastodon_follower_lists[i] = mastodon_status_id_to_followers_list(ids[i]);
			cout << "Mastodon status id: " << ids[i] << ", endorsement_score: "
			     << rms(mastodon_follower_lists[i]);
		}
		cout << endl;
		return 0;
	}

	// @TODO rename to tweet info?
	vector<TweetInfo> tweet_info_list =
		// @TODO generalize
		(backend == Backend::TWITTER) ?
			tweet_ids_to_tweet_info_list(ids) : vector<TweetInfo> {};

	std::sort(
		tweet_info_list.begin(),
		tweet_info_list.end(),
		[](const auto &lhs, const auto& rhs) {
			return lhs.endorsement_score > rhs.endorsement_score;
			// return lhs.composite_score > rhs.composite_score;
		}
	);

	// @TODO generalize for both twitter and mastodon
	if (backend == Backend::TWITTER) {
		cout << "---------------------------" << endl;
		for (auto display_info : tweet_info_list) {

			cout << BOLD_ON << display_info.author_display_name << " (@"
			     << display_info.author_username << "):" << BOLD_OFF << endl;
			cout << endl;
			cout << display_info.text << endl;
			cout << endl;
			cout << "Link to original tweet: " << display_info.url << endl;
			// @NOTE name constants?
			if (display_info.like_count > 5'000) {
				cout << BOLD_ON << "Warning: " << BOLD_OFF;
				if (display_info.like_count > 50'000) {
					cout << "Very high like count.";
				} else {
					cout << "High like count.";
				}
				// @NOTE can increase number of pages returned for more accurate results
				// but that consume more API calls
				cout << " Endorsement score may be variable or inaccurate." << endl;
			}
			// @TODO name constant
			else if (display_info.like_count <= 10) {
				cout << BOLD_ON "Warning: " BOLD_OFF 
					"low like count. Endorsement score may be variable or"
					" inaccurate" << endl;
			}
			cout << "Endorsement score: " << display_info.endorsement_score << " | ";
			// cout << "likes: " << display_info.like_count << ", ";
			// cout << "shares: " << display_info.share_count << endl;
			// @TEST
			// cout << "Composite score: " << display_info.composite_score << " | ";
			cout << "Likes: " << display_info.like_count << endl;

			// @TODO this needs to be improved
			if (!display_info.was_reached_before_rate_limit) {
				// @TODO just wait in future when maxed out, tell user they can quit
				// with control-c, then calculate additional tweets after 15min
				cout << BOLD_ON "Warning: " << BOLD_OFF "Twitter API rate limit "
					"reached. For an accurate endorsement score for this tweet, please "
					"try again in ~15min. If you input more than 10-15 tweet ids with "
					"high like counts, you are likely to max out the rate limit."
					<< endl;
			}
			cout << "---------------------------" << endl;
		}

		cout << BOLD_ON "^ scroll to top for tweet with highest endorsement score"
		     << BOLD_OFF << endl;
	}

	// @TODO re-add suppot for Mastodon?
	// https://mastodon.example/api/v1/statuses/:id
}

// @TODO refine score calculation
double rms(vector<long long> array) {
	if (array.size() < 2) {
		return 0;
	}

	// @TODO test
	long long sum = 0;
#if 0
	for (auto element : array) {
		sum += element*element;
	}
#endif
#if 1
	//@NOTE skip max element, possibly more accurate for small follower lists
	std::sort(array.begin(), array.end());
	for (int i = 0; i < array.size() - 1; ++i) {
		sum += array[i] * array[i];
	}
#endif
	return sqrt((double) sum / (double) array.size());
}

