//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include "stdafx.h"
#include "LastFm.h"
#include <curl/curl.h>
#include <openssl/md5.h>
#include <core/support/Preferences.h>
#include <core/io/HttpClient.h>
#include <core/support/PreferenceKeys.h>
#include <app/util/PreferenceKeys.h>
#include <json.hpp>
#include <sstream>
#include <map>

/* http://www.last.fm/group/Last.fm+Web+Services/forum/21604/_/522900 -- it's ok to
put our key in the code */
static const std::string API_KEY = "8b7b7369cd92bbcc071c5f8a1de1d287";
static const std::string API_SECRET = "6dc09da925fe5c115b90320213c53b46";
static const std::string URL_BASE = "http://ws.audioscrobbler.com/2.0/";
static const std::string GET_TOKEN = "auth.getToken";
static const std::string GET_SESSION = "auth.getSession";
static const std::string ACCOUNT_LINK_URL_BASE = "http://www.last.fm/api/auth/?api_key=" + API_KEY + "&token=";

using namespace musik;
using namespace musik::core::prefs;

using LastFmClient = musik::core::io::HttpClient<std::stringstream>;
using Preferences = musik::core::Preferences;
using Prefs = std::shared_ptr<Preferences>;

static std::unique_ptr<LastFmClient> createClient() {
    return LastFmClient::Create(std::stringstream());
}

static void validate(musik::cube::lastfm::Session& session) {
    session.valid =
        session.sessionId.size() &&
        session.username.size() &&
        session.token.size();
}

static std::string generateSignedUrl(
    const std::string& method,
    std::map<std::string, std::string>&& params = { })
{
    params["method"] = method;
    params["api_key"] = API_KEY;

    std::string toHash;
    std::string url = URL_BASE;
    bool first = true;

    for (auto it : params) {
        toHash += it.first + it.second;
        url += (first ? "?" : "&") + it.first + "=" + it.second;
        first = false;
    }

    toHash += API_SECRET;

    /* compute the sum */
    unsigned char rawDigest[MD5_DIGEST_LENGTH];
    MD5((const unsigned char*)toHash.c_str(), toHash.length(), rawDigest);

    /* convert to hex */
    char hexDigest[33];
    for (size_t i = 0; i < 16; i++) {
        snprintf(&(hexDigest[i * 2]), 16 * 2, "%02x", (unsigned int)rawDigest[i]);
    }
    hexDigest[32] = 0;

    url += "&format=json&api_sig=" + std::string(hexDigest);

    return url;
}

static inline Prefs settings() {
    return Preferences::ForComponent(components::Settings);
}

namespace musik { namespace cube { namespace lastfm {

    const std::string CreateAccountLinkToken() {
        std::string url = generateSignedUrl(GET_TOKEN);
        std::string token;

        auto client = createClient();
        client->Url(url)
            .Mode(LastFmClient::Thread::Current)
            .Run([&token](LastFmClient* client, int statusCode, CURLcode curlCode) {
                if (statusCode == 200) {
                    try {
                        auto json = nlohmann::json::parse(client->Stream().str());
                        token = json.value("token", "");
                    }
                    catch (...) {
                        /* not much we can do... */
                    }
                }
        });

        return token;
    }

    extern Session CreateSession(const std::string& token) {
        std::string url = generateSignedUrl(GET_SESSION, { { "token", token } });

        Session session;
        session.token = token;

        auto client = createClient();
        client->Url(url)
            .Mode(LastFmClient::Thread::Current)
            .Run([&session](LastFmClient* client, int statusCode, CURLcode curlCode) {
                if (statusCode == 200) {
                    try {
                        auto json = nlohmann::json::parse(client->Stream().str());
                        auto subscriber = json["session"];
                        session.username = subscriber.value("name", "");
                        session.sessionId = subscriber.value("key", "");
                    }
                    catch (...) {
                        /* not much we can do... */
                    }
                }
        });

        validate(session);
        return session;
    }

    const std::string CreateLinkUrl(const std::string& token) {
        return ACCOUNT_LINK_URL_BASE + token;
    }

    Session LoadSession() {
        namespace keys = cube::prefs::keys;
        auto prefs = settings();
        Session session;
        session.token = prefs->GetString(keys::LastFmToken);
        session.sessionId = prefs->GetString(keys::LastFmSessionId);
        session.username = prefs->GetString(keys::LastFmUsername);
        validate(session);
        return session;
    }

    void SaveSession(const Session& session) {
        namespace keys = cube::prefs::keys;
        auto prefs = settings();
        prefs->SetString(keys::LastFmToken.c_str(), session.token.c_str());
        prefs->SetString(keys::LastFmSessionId.c_str(), session.sessionId.c_str());
        prefs->SetString(keys::LastFmUsername.c_str(), session.username.c_str());
    }

    void ClearSession() {
        Session session;
        SaveSession(session);
    }

} } }