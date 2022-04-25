#include "endorsement_score.h"

#include <cmath>

using std::string;
using json = nlohmann::json;

using std::string;
using std::vector;

using std::cout;
using std::cerr;
using std::endl;

string TWITTER_BEARER_TOKEN;
string TWITTER_CONSUMER_KEY;
string TWITTER_CONSUMER_SECRET;

// @TODO does this need to be global?
// @TODO make static or use namespace
int API_CALLS_CONSUMED = 0;

nlohmann::basic_json<>
http_single_request_to_json(
const string url, const string bearer_token /*= ""*/) {
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
	int number_of_pages/* = TWITTER_MAX_LIKING_USERS_PAGES*/
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