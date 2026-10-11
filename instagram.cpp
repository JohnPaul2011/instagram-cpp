// instagram.cpp - FIXED VERSION
#include "instagram.hpp"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <ctime>
#include <thread>
#include <iostream>  // Added for std::cout, std::cerr, std::cin
#include <iomanip>

static const std::vector<std::string> DEFAULT_USER_AGENTS = {
    "Mozilla/5.0 (iPhone; CPU iPhone OS 15_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/15.0 Mobile/15E148 Safari/604.1",
    "Mozilla/5.0 (iPhone; CPU iPhone OS 16_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.0 Mobile/15E148 Safari/604.1",
    "Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.0 Mobile/15E148 Safari/604.1",
    "Mozilla/5.0 (Linux; Android 12; SM-S901B) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/112.0.0.0 Mobile Safari/537.36",
    "Mozilla/5.0 (Linux; Android 13; Pixel 7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/112.0.0.0 Mobile Safari/537.36",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/112.0.0.0 Safari/537.36"
};

// ==================== INSTAGRAM CLASS IMPLEMENTATION ====================

Instagram::Instagram()
    : curl(nullptr), logged_in(false), current_ua_index(0), ua_rotation_count(0), ua_rotation_interval(5) {
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    }
    set_user_agent_rotation(DEFAULT_USER_AGENTS, 5);
    last_request_time = std::chrono::steady_clock::now();
    two_factor_handler = std::make_shared<ConsoleTwoFactorHandler>();
    rate_config = {2000, 10000, 5, 2};
    proxy_config.enabled = false;
}

Instagram::~Instagram() {
    if (curl) curl_easy_cleanup(curl);
    curl_global_cleanup();
}

void Instagram::configure_proxy() {
    if (!proxy_config.enabled || !curl) return;

    if (proxy_config.type == "http" || proxy_config.type == "https") {
        curl_easy_setopt(curl, CURLOPT_PROXY, proxy_config.host.c_str());
        curl_easy_setopt(curl, CURLOPT_PROXYPORT, proxy_config.port);
        if (!proxy_config.username.empty()) {
            curl_easy_setopt(curl, CURLOPT_PROXYUSERNAME, proxy_config.username.c_str());
            curl_easy_setopt(curl, CURLOPT_PROXYPASSWORD, proxy_config.password.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
    }
    else if (proxy_config.type == "socks4") {
        curl_easy_setopt(curl, CURLOPT_PROXY, proxy_config.host.c_str());
        curl_easy_setopt(curl, CURLOPT_PROXYPORT, proxy_config.port);
        curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS4);
    }
    else if (proxy_config.type == "socks5") {
        curl_easy_setopt(curl, CURLOPT_PROXY, proxy_config.host.c_str());
        curl_easy_setopt(curl, CURLOPT_PROXYPORT, proxy_config.port);
        if (!proxy_config.username.empty()) {
            curl_easy_setopt(curl, CURLOPT_PROXYUSERNAME, proxy_config.username.c_str());
            curl_easy_setopt(curl, CURLOPT_PROXYPASSWORD, proxy_config.password.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5);
    }
}

void Instagram::set_proxy(const ProxyConfig& proxy) {
    proxy_config = proxy;
    if (curl) configure_proxy();
}

void Instagram::disable_proxy() {
    proxy_config.enabled = false;
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_PROXY, "");
        curl_easy_setopt(curl, CURLOPT_PROXYPORT, 0);
        curl_easy_setopt(curl, CURLOPT_PROXYUSERNAME, "");
        curl_easy_setopt(curl, CURLOPT_PROXYPASSWORD, "");
    }
}

size_t Instagram::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t total_size = size * nmemb;
    output->append((char*)contents, total_size);
    return total_size;
}

void Instagram::parse_cookies_from_response(const CurlResponse& response) {
    std::regex cookie_regex(R"(Set-Cookie:\s*([^;]+))", std::regex_constants::icase);
    std::smatch matches;
    std::string::const_iterator search_start(response.data.cbegin());
    while (std::regex_search(search_start, response.data.cend(), matches, cookie_regex)) {
        std::string cookie = matches[1].str();
        size_t eq_pos = cookie.find('=');
        if (eq_pos != std::string::npos) {
            std::string name = cookie.substr(0, eq_pos);
            std::string value = cookie.substr(eq_pos + 1);
            name.erase(0, name.find_first_not_of(" \t"));
            name.erase(name.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            if (!name.empty()) {
                cookies[name] = value;
                if (name == "sessionid") session_id = value;
                if (name == "csrftoken") csrf_token = value;
            }
        }
        search_start = matches[0].second;
    }
}

std::string Instagram::get_cookie_string() const {
    std::string result;
    for (const auto& [name, value] : cookies) {
        if (!result.empty()) result += "; ";
        result += name + "=" + value;
    }
    return result;
}

void Instagram::apply_rate_limit() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_request_time).count();
    if (elapsed < rate_config.min_delay_ms) {
        int delay = rate_config.min_delay_ms - elapsed;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, delay / 2);
        delay += dis(gen);
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    }
    last_request_time = std::chrono::steady_clock::now();
}

bool Instagram::is_rate_limited(const CurlResponse& response) {
    if (response.http_code == 429) return true;
    if (response.data.find("rate limit") != std::string::npos) return true;
    if (response.data.find("too many requests") != std::string::npos) return true;
    if (response.data.find("Please wait a few minutes") != std::string::npos) return true;
    return false;
}

void Instagram::rotate_user_agent() {
    if (user_agents.empty()) return;
    ua_rotation_count++;
    if (ua_rotation_count >= ua_rotation_interval) {
        current_ua_index = (current_ua_index + 1) % user_agents.size();
        if (curl) curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agents[current_ua_index].c_str());
        ua_rotation_count = 0;
    }
}

Instagram::CurlResponse Instagram::request(const std::string& url, const std::string& method,
                                          const std::string& post_data, const std::map<std::string, std::string>& extra_headers) {
    CurlResponse response;
    int retry_count = 0;

    while (retry_count <= rate_config.max_retries) {
        apply_rate_limit();
        rotate_user_agent();

        if (!curl) throw InstagramException("CURL not initialized");

        std::string response_data;
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "X-IG-App-ID: 936619743392459");
        headers = curl_slist_append(headers, "Accept: */*");
        headers = curl_slist_append(headers, "Accept-Language: en-US,en;q=0.9");
        headers = curl_slist_append(headers, "X-Requested-With: XMLHttpRequest");
        headers = curl_slist_append(headers, ("X-CSRFToken: " + csrf_token).c_str());
        headers = curl_slist_append(headers, ("Cookie: " + get_cookie_string()).c_str());

        for (const auto& [key, value] : extra_headers) {
            headers = curl_slist_append(headers, (key + ": " + value).c_str());
        }

        if (method == "POST" && !post_data.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
            headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
        }

        CURLcode res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.http_code);
        response.data = response_data;
        parse_cookies_from_response(response);
        curl_slist_free_all(headers);

        if (res != CURLE_OK) {
            if (retry_count >= rate_config.max_retries) {
                throw InstagramException("CURL error: " + std::string(curl_easy_strerror(res)));
            }
            retry_count++;
            std::this_thread::sleep_for(std::chrono::milliseconds(
                rate_config.min_delay_ms * rate_config.backoff_multiplier * retry_count));
            continue;
        }

        if (is_rate_limited(response)) {
            if (retry_count >= rate_config.max_retries) {
                throw InstagramException("Rate limited by Instagram. Try again later.");
            }
            retry_count++;
            int delay = rate_config.min_delay_ms * rate_config.backoff_multiplier * retry_count;
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            continue;
        }
        break;
    }
    return response;
}

std::string Instagram::extract_csrf_token(const std::string& html) {
    std::regex csrf_regex(R"(name=\"csrf_token\"\s+value=\"([^\"]+)\")");
    std::smatch matches;
    if (std::regex_search(html, matches, csrf_regex) && matches.size() > 1) {
        return matches[1].str();
    }
    return "";
}

std::string Instagram::extract_checkpoint_token(const std::string& html) {
    std::regex checkpoint_regex(R"(window\._sharedData\s*=\s*\{[^}]*\"entry_data\":\{[^}]*\"Challenge\":\[[^\]]*\"checkpoint_url\":\"[^\"]*/checkpoint/(?:transitive/)?([^\"]+))");
    std::smatch matches;
    if (std::regex_search(html, matches, checkpoint_regex) && matches.size() > 1) {
        return matches[1].str();
    }
    return "";
}

Json::Value Instagram::parse_json_from_html(const std::string& html) {
    std::regex json_regex(R"(window\._sharedData\s*=\s*({.*?});</script>)");
    std::smatch matches;
    if (std::regex_search(html, matches, json_regex) && matches.size() > 1) {
        std::string json_str = matches[1].str();
        Json::Value root;
        Json::CharReaderBuilder builder;
        std::string errs;
        std::istringstream iss(json_str);
        if (Json::parseFromStream(builder, iss, &root, &errs)) {
            return root;
        }
    }
    throw InstagramException("Failed to parse JSON from HTML");
}

Json::Value Instagram::fetch_profile_page(const std::string& username) {
    std::string url = "https://www.instagram.com/" + username + "/?__a=1";
    auto response = request(url, "GET");
    if (response.http_code != 200) {
        throw InstagramException("Failed to fetch profile page: HTTP " + std::to_string(response.http_code));
    }
    return parse_json_from_html(response.data);
}

Json::Value Instagram::fetch_graphql(const std::string& query, const Json::Value& variables) {
    if (!logged_in) throw InstagramException("Login required for GraphQL queries");

    Json::Value payload;
    payload["query"] = query;
    payload["variables"] = variables;
    std::string post_data = payload.toStyledString();

    std::map<std::string, std::string> headers;
    headers["X-IG-App-ID"] = "936619743392459";
    headers["Referer"] = "https://www.instagram.com/";
    headers["Origin"] = "https://www.instagram.com/";

    auto response = request("https://www.instagram.com/api/graphql", "POST", post_data, headers);
    if (response.http_code != 200) {
        throw InstagramException("GraphQL request failed: HTTP " + std::to_string(response.http_code));
    }

    Json::Value result;
    Json::CharReaderBuilder builder;
    std::string errs;
    std::istringstream iss(response.data);
    if (!Json::parseFromStream(builder, iss, &result, &errs)) {
        throw InstagramException("Failed to parse GraphQL response: " + errs);
    }
    if (result.isMember("errors")) {
        throw InstagramException("GraphQL error: " + result["errors"].asString());
    }
    return result;
}

Json::Value Instagram::login_flow(const std::string& username, const std::string& password) {
    auto response = request("https://www.instagram.com/accounts/login/", "GET");
    if (response.http_code != 200) {
        throw InstagramException("Failed to fetch login page");
    }

    csrf_token = extract_csrf_token(response.data);
    if (csrf_token.empty()) {
        throw InstagramException("Failed to extract CSRF token");
    }

    std::string post_data =
        "username=" + username +
        "&enc_password=#PWD_INSTAGRAM_BROWSER:0:" + std::to_string(time(nullptr)) + ":" + password +
        "&queryParams={}" +
        "&optIntoOneTap=false" +
        "&stopDeletionNonce=false";

    std::map<std::string, std::string> headers;
    headers["X-CSRFToken"] = csrf_token;
    headers["X-IG-App-ID"] = "936619743392459";
    headers["Referer"] = "https://www.instagram.com/accounts/login/";
    headers["Origin"] = "https://www.instagram.com/";
    headers["X-Requested-With"] = "XMLHttpRequest";

    response = request("https://www.instagram.com/accounts/login/ajax/", "POST", post_data, headers);
    if (response.http_code != 200) {
        throw InstagramException("Login failed: HTTP " + std::to_string(response.http_code));
    }

    Json::Value result;
    Json::CharReaderBuilder builder;
    std::string errs;
    std::istringstream iss(response.data);
    if (!Json::parseFromStream(builder, iss, &result, &errs)) {
        throw InstagramException("Failed to parse login response: " + errs);
    }

    if (result.isMember("message") && result["message"].asString() == "checkpoint_required") {
        if (result.isMember("checkpoint_url")) {
            return result;
        }
        throw InstagramException("Checkpoint required but no URL provided");
    }

    if (!result.isMember("authenticated") || !result["authenticated"].asBool()) {
        std::string message = result.isMember("message") ? result["message"].asString() : "Unknown error";
        throw InstagramException("Login failed: " + message);
    }

    if (result.isMember("user")) {
        user_id = result["user"]["pk"].asString();
    }

    logged_in = true;
    return result;
}

void Instagram::handle_checkpoint(const std::string& checkpoint_url,
                                const std::string& username,
                                const std::string& password) {
    std::string checkpoint_token = extract_checkpoint_token(checkpoint_url);
    if (checkpoint_token.empty()) {
        throw InstagramException("Failed to extract checkpoint token from URL: " + checkpoint_url);
    }

    std::string verification_code = two_factor_handler->get_verification_code(checkpoint_url);
    if (verification_code.empty()) {
        throw InstagramException("No verification code provided");
    }

    std::string post_data =
        "username=" + username +
        "&enc_password=#PWD_INSTAGRAM_BROWSER:0:" + std::to_string(time(nullptr)) + ":" + password +
        "&checkpoint_token=" + checkpoint_token +
        "&verification_code=" + verification_code;

    std::map<std::string, std::string> headers;
    headers["X-CSRFToken"] = csrf_token;
    headers["Referer"] = checkpoint_url;
    headers["Origin"] = "https://www.instagram.com/";

    auto response = request("https://www.instagram.com/api/v1/web/accounts/checkpoint/verify/", "POST", post_data, headers);
    if (response.http_code != 200) {
        throw InstagramException("2FA verification failed: HTTP " + std::to_string(response.http_code));
    }

    Json::Value result;
    Json::CharReaderBuilder builder;
    std::string errs;
    std::istringstream iss(response.data);
    if (!Json::parseFromStream(builder, iss, &result, &errs)) {
        throw InstagramException("Failed to parse 2FA response: " + errs);
    }

    if (!result.isMember("authenticated") || !result["authenticated"].asBool()) {
        std::string message = result.isMember("message") ? result["message"].asString() : "Unknown error";
        throw InstagramException("2FA verification failed: " + message);
    }

    logged_in = true;
}

bool Instagram::login(const std::string& username, const std::string& password) {
    return login_with_2fa(username, password, "");
}

bool Instagram::login_with_2fa(const std::string& username,
                              const std::string& password,
                              const std::string& verification_code) {
    try {
        Json::Value result = login_flow(username, password);

        if (result.isMember("message") && result["message"].asString() == "checkpoint_required") {
            if (!verification_code.empty()) {
                if (result.isMember("checkpoint_url")) {
                    handle_checkpoint(result["checkpoint_url"].asString(), username, password);
                } else {
                    throw InstagramException("Checkpoint required but no URL in response");
                }
            } else {
                return false;
            }
        }

        return logged_in;
    } catch (const InstagramException& e) {
        std::cerr << "Login error: " << e.what() << std::endl;
        return false;
    }
}

void Instagram::load_session(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw InstagramException("Failed to open session file: " + filename);
    }

    Json::Value session;
    Json::CharReaderBuilder builder;
    std::string errs;
    if (!Json::parseFromStream(builder, file, &session, &errs)) {
        throw InstagramException("Failed to parse session file: " + errs);
    }

    if (session.isMember("sessionid")) {
        session_id = session["sessionid"].asString();
        cookies["sessionid"] = session_id;
    }
    if (session.isMember("csrftoken")) {
        csrf_token = session["csrftoken"].asString();
        cookies["csrftoken"] = csrf_token;
    }
    if (session.isMember("user_id")) {
        user_id = session["user_id"].asString();
    }

    logged_in = session.isMember("sessionid") && !session_id.empty();
}

void Instagram::save_session(const std::string& filename) const {
    Json::Value session;
    session["sessionid"] = session_id;
    session["csrftoken"] = csrf_token;
    session["user_id"] = user_id;
    session["logged_in"] = logged_in;

    std::ofstream file(filename);
    if (!file.is_open()) {
        throw InstagramException("Failed to save session file: " + filename);
    }
    file << session.toStyledString();
}

void Instagram::logout() {
    session_id.clear();
    csrf_token.clear();
    user_id.clear();
    cookies.clear();
    logged_in = false;
}

void Instagram::set_rate_limit(const RateLimitConfig& config) {
    rate_config = config;
}

void Instagram::set_two_factor_handler(std::shared_ptr<TwoFactorHandler> handler) {
    two_factor_handler = handler;
}

void Instagram::set_user_agent_rotation(const std::vector<std::string>& agents, size_t interval) {
    user_agents = agents;
    ua_rotation_interval = interval;
    if (!user_agents.empty()) {
        current_ua_index = rand() % user_agents.size();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agents[current_ua_index].c_str());
        }
    }
}

Profile Instagram::get_profile(const std::string& username) {
    Json::Value data = fetch_profile_page(username);

    if (!data.isMember("entry_data") || !data["entry_data"].isMember("ProfilePage")) {
        throw InstagramException("Profile not found or private");
    }

    Json::Value profile_data = data["entry_data"]["ProfilePage"][0]["graphql"]["user"];
    return Profile::from_json(profile_data);
}

std::vector<Profile> Instagram::get_followers(const std::string& username, size_t limit) {
    if (!logged_in) {
        throw InstagramException("Login required to get followers");
    }

    Profile profile = get_profile(username);
    std::vector<Profile> followers;

    std::string query = R"(
        query FollowersQuery($id: ID!, $first: Int!) {
            user(id: $id) {
                edge_followed_by {
                    edges {
                        node {
                            username
                            full_name
                            pk
                            profile_pic_url
                            is_verified
                        }
                    }
                }
            }
        }
    )";

    Json::Value variables;
    variables["id"] = profile.pk_id;
    variables["first"] = static_cast<Json::Int64>(limit);

    Json::Value result = fetch_graphql(query, variables);

    if (result.isMember("data") && result["data"].isMember("user")) {
        Json::Value edges = result["data"]["user"]["edge_followed_by"]["edges"];
        for (Json::Value::ArrayIndex i = 0; i < edges.size() && i < limit; i++) {
            followers.push_back(Profile::from_json(edges[i]["node"]));
        }
    }

    return followers;
}

std::vector<Profile> Instagram::get_following(const std::string& username, size_t limit) {
    if (!logged_in) {
        throw InstagramException("Login required to get following");
    }

    Profile profile = get_profile(username);
    std::vector<Profile> following;

    std::string query = R"(
        query FollowingQuery($id: ID!, $first: Int!) {
            user(id: $id) {
                edge_follow {
                    edges {
                        node {
                            username
                            full_name
                            pk
                            profile_pic_url
                            is_verified
                        }
                    }
                }
            }
        }
    )";

    Json::Value variables;
    variables["id"] = profile.pk_id;
    variables["first"] = static_cast<Json::Int64>(limit);

    Json::Value result = fetch_graphql(query, variables);

    if (result.isMember("data") && result["data"].isMember("user")) {
        Json::Value edges = result["data"]["user"]["edge_follow"]["edges"];
        for (Json::Value::ArrayIndex i = 0; i < edges.size() && i < limit; i++) {
            following.push_back(Profile::from_json(edges[i]["node"]));
        }
    }

    return following;
}

std::vector<Post> Instagram::get_posts(const std::string& username, size_t limit) {
    Json::Value data = fetch_profile_page(username);

    if (!data.isMember("entry_data") || !data["entry_data"].isMember("ProfilePage")) {
        throw InstagramException("Profile not found or private");
    }

    std::vector<Post> posts;
    Json::Value edges = data["entry_data"]["ProfilePage"][0]["graphql"]["user"]["edge_owner_to_timeline_media"]["edges"];

    for (Json::Value::ArrayIndex i = 0; i < edges.size() && i < limit; i++) {
        posts.push_back(Post::from_json(edges[i]["node"]));
    }

    return posts;
}

std::vector<Post> Instagram::get_hashtag_posts(const std::string& hashtag, size_t limit) {
    std::string url = "https://www.instagram.com/explore/tags/" + hashtag + "/?__a=1";
    auto response = request(url, "GET");

    if (response.http_code != 200) {
        throw InstagramException("Failed to fetch hashtag page");
    }

    Json::Value data;
    Json::CharReaderBuilder builder;
    std::string errs;
    std::istringstream iss(response.data);

    if (!Json::parseFromStream(builder, iss, &data, &errs)) {
        throw InstagramException("Failed to parse hashtag response: " + errs);
    }

    std::vector<Post> posts;
    if (data.isMember("graphql") && data["graphql"].isMember("hashtag")) {
        Json::Value edges = data["graphql"]["hashtag"]["edge_hashtag_to_media"]["edges"];
        for (Json::Value::ArrayIndex i = 0; i < edges.size() && i < limit; i++) {
            posts.push_back(Post::from_json(edges[i]["node"]));
        }
    }

    return posts;
}

Post Instagram::get_post_by_shortcode(const std::string& shortcode) {
    std::string url = "https://www.instagram.com/p/" + shortcode + "/?__a=1";
    auto response = request(url, "GET");

    if (response.http_code != 200) {
        throw InstagramException("Failed to fetch post");
    }

    Json::Value data;
    Json::CharReaderBuilder builder;
    std::string errs;
    std::istringstream iss(response.data);

    if (!Json::parseFromStream(builder, iss, &data, &errs)) {
        throw InstagramException("Failed to parse post response: " + errs);
    }

    if (data.isMember("graphql") && data["graphql"].isMember("shortcode_media")) {
        return Post::from_json(data["graphql"]["shortcode_media"]);
    }

    throw InstagramException("Post not found");
}

std::vector<Post> Instagram::get_stories(const std::string& username) {
    if (!logged_in) {
        throw InstagramException("Login required to get stories");
    }

    Profile profile = get_profile(username);
    std::vector<Post> stories;

    std::string query = R"(
        query StoryQuery($user_id: ID!) {
            user(id: $user_id) {
                edge_user_to_story {
                    edges {
                        node {
                            id
                            shortcode
                            display_url
                            video_url
                            is_video
                            taken_at_timestamp
                            edge_media_to_caption {
                                edges {
                                    node {
                                        text
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    )";

    Json::Value variables;
    variables["user_id"] = profile.pk_id;

    Json::Value result = fetch_graphql(query, variables);

    if (result.isMember("data") && result["data"].isMember("user")) {
        Json::Value edges = result["data"]["user"]["edge_user_to_story"]["edges"];
        for (Json::Value::ArrayIndex i = 0; i < edges.size(); i++) {
            stories.push_back(Post::from_json(edges[i]["node"]));
        }
    }

    return stories;
}

std::string Instagram::download_media(const std::string& url, const std::string& filename) {
    auto response = request(url, "GET");

    if (response.http_code != 200) {
        throw InstagramException("Failed to download media: HTTP " + std::to_string(response.http_code));
    }

    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        throw InstagramException("Failed to create file: " + filename);
    }

    file.write(response.data.data(), response.data.size());
    return filename;
}

Json::Value Profile::to_json() const {
    Json::Value json;
    json["username"] = username;
    json["full_name"] = full_name;
    json["biography"] = biography;
    json["profile_pic_url"] = profile_pic_url;
    json["external_url"] = external_url;
    json["is_private"] = is_private;
    json["is_verified"] = is_verified;
    json["followers_count"] = static_cast<Json::Int64>(followers_count);
    json["following_count"] = static_cast<Json::Int64>(following_count);
    json["media_count"] = static_cast<Json::Int64>(media_count);
    json["pk_id"] = pk_id;
    return json;
}

Profile Profile::from_json(const Json::Value& json) {
    Profile profile;
    profile.username = json.get("username", "").asString();
    profile.full_name = json.get("full_name", "").asString();
    profile.biography = json.get("biography", "").asString();
    profile.profile_pic_url = json.get("profile_pic_url", "").asString();
    profile.external_url = json.get("external_url", "").asString();
    profile.is_private = json.get("is_private", false).asBool();
    profile.is_verified = json.get("is_verified", false).asBool();

    if (json.isMember("edge_followed_by")) {
        profile.followers_count = json["edge_followed_by"].get("count", 0).asInt();
    } else {
        profile.followers_count = json.get("followers_count", 0).asInt();
    }

    if (json.isMember("edge_follow")) {
        profile.following_count = json["edge_follow"].get("count", 0).asInt();
    } else {
        profile.following_count = json.get("following_count", 0).asInt();
    }

    if (json.isMember("edge_owner_to_timeline_media")) {
        profile.media_count = json["edge_owner_to_timeline_media"].get("count", 0).asInt();
    } else {
        profile.media_count = json.get("media_count", 0).asInt();
    }

    profile.pk_id = json.get("id", "").asString();
    if (profile.pk_id.empty()) {
        profile.pk_id = json.get("pk", "").asString();
    }

    return profile;
}

Json::Value Post::to_json() const {
    Json::Value json;
    json["shortcode"] = shortcode;
    json["pk_id"] = pk_id;
    json["owner_username"] = owner_username;
    json["owner_pk_id"] = owner_pk_id;
    json["caption"] = caption;
    json["like_count"] = static_cast<Json::Int64>(like_count);
    json["comment_count"] = static_cast<Json::Int64>(comment_count);
    json["view_count"] = static_cast<Json::Int64>(view_count);
    json["is_video"] = is_video;
    json["date_posted"] = date_posted;

    Json::Value images(Json::arrayValue);
    for (const auto& url : image_urls) {
        images.append(url);
    }
    json["image_urls"] = images;

    json["video_url"] = video_url;

    Json::Value thumbnails(Json::arrayValue);
    for (const auto& url : thumbnail_urls) {
        thumbnails.append(url);
    }
    json["thumbnail_urls"] = thumbnails;

    return json;
}

Post Post::from_json(const Json::Value& node) {
    Post post;

    post.shortcode = node.get("shortcode", "").asString();
    post.pk_id = node.get("id", "").asString();
    post.is_video = node.get("is_video", false).asBool();

    if (node.isMember("owner")) {
        post.owner_username = node["owner"].get("username", "").asString();
        post.owner_pk_id = node["owner"].get("id", "").asString();
    }

    if (node.isMember("edge_media_to_caption") &&
        node["edge_media_to_caption"].isMember("edges") &&
        !node["edge_media_to_caption"]["edges"].empty()) {
        post.caption = node["edge_media_to_caption"]["edges"][0]["node"].get("text", "").asString();
    } else if (node.isMember("caption")) {
        post.caption = node.get("caption", "").asString();
    }

    if (node.isMember("edge_liked_by")) {
        post.like_count = node["edge_liked_by"].get("count", 0).asInt();
    } else {
        post.like_count = node.get("like_count", 0).asInt();
    }

    if (node.isMember("edge_media_to_comment")) {
        post.comment_count = node["edge_media_to_comment"].get("count", 0).asInt();
    } else {
        post.comment_count = node.get("comment_count", 0).asInt();
    }

    post.view_count = node.get("video_view_count", 0).asInt();

    if (node.isMember("taken_at_timestamp")) {
        time_t timestamp = static_cast<time_t>(node["taken_at_timestamp"].asInt());
        char buffer[80];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", gmtime(&timestamp));
        post.date_posted = buffer;
    }

    if (post.is_video) {
        post.video_url = node.get("video_url", "").asString();
        if (node.isMember("thumbnail_src")) {
            post.thumbnail_urls.push_back(node["thumbnail_src"].asString());
        }
    } else {
        if (node.isMember("display_url")) {
            post.image_urls.push_back(node["display_url"].asString());
        }
    }

    if (node.isMember("edge_sidecar_to_children")) {
        for (Json::Value::ArrayIndex i = 0; i < node["edge_sidecar_to_children"]["edges"].size(); i++) {
            Json::Value child_node = node["edge_sidecar_to_children"]["edges"][i]["node"];
            if (child_node.get("is_video", false).asBool()) {
                if (!child_node.get("video_url", "").asString().empty()) {
                    post.video_url = child_node["video_url"].asString();
                }
            } else {
                if (!child_node.get("display_url", "").asString().empty()) {
                    post.image_urls.push_back(child_node["display_url"].asString());
                }
            }
        }
    }

    return post;
}

std::string ConsoleTwoFactorHandler::get_verification_code(const std::string& checkpoint_url) {
    std::cout << "\n=== 2FA Required ===" << std::endl;
    std::cout << "Checkpoint URL: " << checkpoint_url << std::endl;
    std::cout << "Please check your Instagram app for a verification code." << std::endl;
    std::cout << "Enter the 6-digit code: ";
    std::string code;
    std::cin >> code;
    return code;
}
