#include "oauth.h"

#include <iostream>
#include <cpr/cpr.h>
#include "util.h"

using std::string;
using std::cout;
using std::endl;
using std::cerr;

// @TODO make these params, not globals!
string request_token_url = "https://api.twitter.com/oauth/request_token";
string request_token_query_args = "oauth_callback=oob";
string authorize_url = "https://api.twitter.com/oauth/authorize";
string access_token_url = "https://api.twitter.com/oauth/access_token";

string
produce_oauth_protected_resource_url
(string consumer_key, string consumer_secret, string oauth_token, string oauth_token_secret) {
	// @TODO make this a parameter
	string oauth_protected_resource =
		"https://api.twitter.com/1.1/statuses/home_timeline.json";
	string oauth_protected_resource_params = "count=200";

	// We assume you have gotten the access token. You may have e.g., used
	// simple_auth to get it.

	OAuth::Consumer consumer(consumer_key, consumer_secret);
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


// @TODO make globals function arguments
string oauth_make_protected_resource_url(string consumer_key, string consumer_secret) {
	// @TODO security audit
	// @NOTE do this in main?
	// @TODO use config file
	// @TODO make these params?
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
		OAuth::Consumer consumer(consumer_key, consumer_secret);
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
		cout << endl;

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
		// cout << endl;
		// cout << "You may now access protected resources using the access tokens "
		//      << "above." << endl;
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
		produce_oauth_protected_resource_url(
			consumer_key, consumer_secret,
			oauth_token_key, oauth_token_secret
		);

	return url;
}