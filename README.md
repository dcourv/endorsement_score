## What is this all about? ##

What if social media algorithms tried not to maximize only engagement, but *reputational engagement*? What if content was promoted not only on the basis of how many people engaged with that content, but also *the reputation of those who engage with that content*? This program is a first hack at trying to evaluate an "endorsement score" of tweets or Mastodon statuses, which attempts to gauge the average reputation of those who have engaged with it.

## Installation ##

[//]: <> (@NOTE: Test whether you need elevated access)
**Important:** In order to use this program, you'll need a Twitter API key. You can get one at the [Twitter developer portal](https://developer.twitter.com/en/portal/petition/essential/basic-info).

First, make sure `cmake` and `ninja` are installed.

Secondly, I recommend installing `cpr` and `nlohmann-json` from your package manager (if available). If you do not have these installed, cmake should compile them from source, but this will take significantly longer to build.

Homebrew:

`brew install cpr nlohmann-json`

Apt (does not have cpr):

`apt install nlohmann-json3-dev`

Yay:

`yay -S cpr nlohmann-json`

Then, clone the repository:
`git clone https://github.com/dcourv/endorsement_score`

And build:

`cd endorsement_score`

`cmake -B build -S . -G Ninja && cmake --build build`

The `endorsement_score` executable will be placed in the `build` directory.

Lastly, put your Twitter API Bearer Token (see above) into a one-line file called `twitter_bearer_token.txt` in the directory that you cloned the repsoitory into.

## Usage ##
`./build/endorsement_score -t [ids]` where `[ids]` is a space-delimited string of Twitter status ids.

Example: `./build/endorsement_score -t 1492120137205526528`

You may also pass `-m` instead of `-t` and use a Mastodon status ids.
