#include <cmath>
#include <vector>
#include <algorithm>
#include <iostream>
#include <locale>
#include <unordered_map>
#include <fstream>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

using std::string;
using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::vector;
using std::unordered_map;

using json = nlohmann::json;

string TWITTER_BEARER_TOKEN;

enum class Backend { TWITTER, MASTODON };

double rms(vector<long long> array);

#define BOLD_ON "\e[1m"
#define BOLD_OFF "\e[0m"

// @TODO (!!!) crawl retweets

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
	}
#if 0
	else {
		// @LOG
		cout << "Request took " << r.elapsed << endl;
		cout << "Body:" << endl << r.text;
	}
#endif

	if (r.text == "") {
		cerr << "Error: empty body text from " << url << endl;
	}
	
	auto json_object = json::parse(r.text);
	// @DEBUG
	// cout << json_object.dump(2) << endl;
	return json_object;
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
	int number_of_pages = 10
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
			vector<long long> followers_list;
			if (info.is_retweet) {
				followers_list =
					tweet_id_to_liking_followers_list(
						info.retweeted_tweet_id
					);
			}
			else {
				followers_list = tweet_id_to_liking_followers_list(info.id);
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

	string help_message =
		BOLD_ON "Usage: " BOLD_OFF "./endorsement_score [-t id ...] [-m id ...]\n"
		"-t id ... : calculate the endorsement scores for tweets with given id[s]"
		"\n"
		"-m id ... : calculate the endorsement scores for Mastodon statuses with"
			" given id[s]\n"
		"You must pass either -t or -m, but not both";

	if (argc < 3) {
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

	vector<string> ids;
	// @TODO validate input?
	for (int i = 2; i < argc; ++i) {
		ids.push_back(argv[i]);
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

	// @TODO handle file not found exception
	std::ifstream file_stream("twitter_bearer_token.txt");
	std::stringstream string_stream;
	string_stream << file_stream.rdbuf();
	TWITTER_BEARER_TOKEN = string_stream.str();
	if (TWITTER_BEARER_TOKEN == "") {
		throw (
			std::runtime_error(
				BOLD_ON "\nError: " BOLD_OFF "failed to find Twitter API bearer token."
				" Please put your entire bearer token in a one-line file "
				"\"twitter_bearer_token.txt\" in this directory."
			)
		);
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
			cout << display_info.author_display_name << " (@"
					 << display_info.author_username << "):" << endl;
			cout << endl;
			cout << display_info.text << endl;
			cout << endl;
			cout << "Link to original tweet: " << display_info.url << endl;
			// @NOTE name constants?
			if (display_info.like_count > 5'000) {
				cout << /*BOLD_ON << */"Warning: "/* << BOLD_OFF*/;
				if (display_info.like_count > 50'000) {
					cout << "Very high like count.";
				} else {
					cout << "High like count.";
				}
				// @NOTE can increase number of pages returned for more accurate results
				// but that consume more API calls
				cout << " Endorsement score may be variable or inaccurate." << endl;
			} else if (display_info.like_count <= 10) {
				cout << "Warning: low like count. Endorsement score may be variable or"
					" inaccurate" << endl;
			}
			cout << "Endorsement score: " << display_info.endorsement_score << " | ";
			// cout << "likes: " << display_info.like_count << ", ";
			// cout << "shares: " << display_info.share_count << endl;
			// @TEST
			// cout << "Composite score: " << display_info.composite_score << " | ";
			cout << "Likes: " << display_info.like_count << endl;
			cout << "---------------------------" << endl;
		}
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

