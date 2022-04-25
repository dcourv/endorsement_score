#pragma once

#include <string>
#include <liboauthcpp/liboauthcpp.h>


std::string oauth_make_protected_resource_url(void);

extern std::string request_token_url;
extern std::string request_token_query_args;
extern std::string authorize_url;
extern std::string access_token_url;

std::string
produce_oauth_protected_resource_url
(std::string consumer_key, std::string consumer_secret, std::string oauth_access_token, std::string oauth_access_token_secret);

std::string oauth_make_protected_resource_url(std::string consumer_key, std::string consumer_secret);