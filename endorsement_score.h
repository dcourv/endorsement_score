#pragma once

#include <iostream>
#include <vector>
#include <nlohmann/json.hpp>
#include <cpr/cpr.h>

#define TWITTER_MAX_LIKING_USERS_PAGES 10
// @NOTE must be less than 200
#define TWITTER_MAX_FETCHED_TWEETS 50
#define TWITTER_LIKING_USERS_RATE_LIMIT 75

// @TODO make static
extern int API_CALLS_CONSUMED;

// @TODO change to some sort of auth context
extern std::string TWITTER_BEARER_TOKEN;
extern std::string TWITTER_CONSUMER_KEY;
extern std::string TWITTER_CONSUMER_SECRET;

nlohmann::basic_json<>
http_single_request_to_json(const std::string url, const std::string bearer_token = "");

std::vector<long long> mastodon_status_id_to_followers_list(const std::string status_id);

std::vector<long long>
tweet_id_to_liking_followers_list(
	const std::string tweet_id,
	int number_of_pages = TWITTER_MAX_LIKING_USERS_PAGES
);

double rms(std::vector<long long> array);