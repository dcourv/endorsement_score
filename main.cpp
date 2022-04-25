#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "endorsement_score.h"
#include "oauth.h"
#include "server.h"
#include "util.h"

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

static const char *TWITTER_BEARER_TOKEN_URI = "twitter_bearer_token.txt";
static const char *TWITTER_CONSUMER_KEY_URI = "twitter_api_key.txt";
static const char *TWITTER_CONSUMER_SECRET_URI = "twitter_api_secret.txt";

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
	// start_server();

// #if 0
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
		string url = oauth_make_protected_resource_url(TWITTER_CONSUMER_KEY, TWITTER_CONSUMER_SECRET);
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
// #endif
}