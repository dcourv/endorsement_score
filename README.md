## What is this all about? ##

What if social media algorithms tried not to maximize only engagement, but *reputational engagement?* What if content was promoted not only on the basis of how many people engaged with that content, but also *the reputation of those who engage with that content?* This program is a first hack at evaluating an "endorsement score" of tweets or Mastodon statuses, which attempts to gauge the average reputation of those who have engaged with it.

[//]: <> (
	Example of a tweets with open_paren relatively close_paren high endorsement scores:
	https://twitter.com/i/web/status/1358929992818642947
	https://twitter.com/i/web/status/1358929992818642947
)

## Installation ##

[//]: <> (@NOTE: Test whether you need elevated access)
**Important:** In order to use this program, you'll need a Twitter API key. You can get one at the [Twitter developer portal](https://developer.twitter.com/en/portal/petition/essential/basic-info).

To install, first make sure you have the build tools `cmake` and `ninja` installed on your machine.

Secondly, I recommend installing dependencies `cpr` and `nlohmann-json` from your package manager (if available). If you do not have these installed, cmake should compile them from source, but it will take significantly longer to build.

Homebrew:

`brew install cpr nlohmann-json`

Apt (does not have cpr):

`apt install nlohmann-json3-dev`

Yay:

`yay -S cpr nlohmann-json`

For the Debian-based distros I tried, it seems you also have to `apt install libssl-dev` in order to build `cpr`.

Clone the repository, recursing submodules:
`git clone https://github.com/dcourv/endorsement_score --recurse-submodules`

And build:

`cd endorsement_score`

`cmake -B build -S . -G Ninja && cmake --build build`

The `endorsement_score` executable will be placed in the `build` directory.

Lastly, put your Twitter API Bearer Token (see above) into a one-line file called `twitter_bearer_token.txt` in the directory that you cloned the repsoitory into. The program will not function without a valid Twitter API Bearer Token.

## Usage ##
`./build/endorsement_score -t [ids]` where `[ids]` is a space-delimited string of Twitter status ids.

Example: `./build/endorsement_score -t 1492120137205526528`

You may also pass `-m` instead of `-t` and use a Mastodon status ids.
