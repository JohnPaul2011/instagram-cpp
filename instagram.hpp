// instagram.hpp - COMPLETE INSTAGRAM C++ CLIENT
// Features: Login, Profiles, Posts, Hashtags, Stories, Followers, Media Download
//           Rate Limiting, User Agent Rotation, Proxy Support (disabled by default), 2FA
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <json/json.h>
#include <curl/curl.h>
#include <chrono>
#include <random>
#include <stdexcept>
#include <thread>

class InstagramException : public std::runtime_error {
public:
    explicit InstagramException(const std::string& msg) : std::runtime_error(msg) {}
};

struct RateLimitConfig {
    int min_delay_ms = 2000;       // Minimum delay between requests (2 seconds)
    int max_delay_ms = 10000;      // Maximum delay
    int max_retries = 5;           // Maximum retry attempts
    int backoff_multiplier = 2;    // Exponential backoff factor
};

struct ProxyConfig {
    bool enabled = false;          // Proxy disabled by default
    std::string type = "http";     // "http", "socks4", "socks5"
    std::string host;
    int port = 0;
    std::string username;
    std::string password;
};

class TwoFactorHandler {
public:
    virtual ~TwoFactorHandler() = default;
    virtual std::string get_verification_code(const std::string& checkpoint_url) = 0;
};

class ConsoleTwoFactorHandler : public TwoFactorHandler {
public:
    std::string get_verification_code(const std::string& checkpoint_url) override;
};

class Profile {
public:
    std::string username;
    std::string full_name;
    std::string biography;
    std::string profile_pic_url;
    std::string external_url;
    bool is_private;
    bool is_verified;
    size_t followers_count;
    size_t following_count;
    size_t media_count;
    std::string pk_id;

    Json::Value to_json() const;
    static Profile from_json(const Json::Value& json);
};

class Post {
public:
    std::string shortcode;
    std::string pk_id;
    std::string owner_username;
    std::string owner_pk_id;
    std::string caption;
    size_t like_count;
    size_t comment_count;
    size_t view_count;
    bool is_video;
    std::string date_posted;
    std::vector<std::string> image_urls;
    std::string video_url;
    std::vector<std::string> thumbnail_urls;

    Json::Value to_json() const;
    static Post from_json(const Json::Value& node);
};

class Instagram {
public:
    Instagram();
    ~Instagram();

    // Authentication
    bool login(const std::string& username, const std::string& password);
    bool login_with_2fa(const std::string& username, const std::string& password,
                       const std::string& verification_code = "");
    void load_session(const std::string& filename);
    void save_session(const std::string& filename) const;
    void logout();
    bool is_logged_in() const { return logged_in; }

    // Profiles
    Profile get_profile(const std::string& username);
    std::vector<Profile> get_followers(const std::string& username, size_t limit = 50);
    std::vector<Profile> get_following(const std::string& username, size_t limit = 50);

    // Posts
    std::vector<Post> get_posts(const std::string& username, size_t limit = 20);
    std::vector<Post> get_hashtag_posts(const std::string& hashtag, size_t limit = 20);
    Post get_post_by_shortcode(const std::string& shortcode);
    std::vector<Post> get_stories(const std::string& username);

    // Media
    std::string download_media(const std::string& url, const std::string& filename);

    // Configuration
    void set_rate_limit(const RateLimitConfig& config);
    void set_two_factor_handler(std::shared_ptr<TwoFactorHandler> handler);
    void set_user_agent_rotation(const std::vector<std::string>& agents, size_t interval = 5);
    void set_proxy(const ProxyConfig& proxy);
    void disable_proxy();
    ProxyConfig get_proxy_config() const { return proxy_config; }

    // Session info
    std::string get_session_id() const { return session_id; }
    std::string get_csrf_token() const { return csrf_token; }

private:
    struct CurlResponse { std::string data; long http_code; };

    CURL* curl;
    std::string session_id;
    std::string csrf_token;
    std::string user_id;
    bool logged_in;
    std::map<std::string, std::string> cookies;
    RateLimitConfig rate_config;
    ProxyConfig proxy_config;
    std::shared_ptr<TwoFactorHandler> two_factor_handler;
    std::vector<std::string> user_agents;
    size_t current_ua_index;
    size_t ua_rotation_count;
    size_t ua_rotation_interval;
    std::chrono::steady_clock::time_point last_request_time;

    // HTTP helpers
    CurlResponse request(const std::string& url, const std::string& method = "GET",
                         const std::string& post_data = "", const std::map<std::string, std::string>& headers = {});
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output);
    void parse_cookies_from_response(const CurlResponse& response);
    std::string get_cookie_string() const;
    void apply_rate_limit();
    bool is_rate_limited(const CurlResponse& response);
    void rotate_user_agent();
    void configure_proxy();

    // Instagram API
    Json::Value fetch_profile_page(const std::string& username);
    Json::Value fetch_graphql(const std::string& query, const Json::Value& variables);
    Json::Value login_flow(const std::string& username, const std::string& password);
    void handle_checkpoint(const std::string& checkpoint_url, const std::string& username, const std::string& password);
    std::string extract_csrf_token(const std::string& html);
    std::string extract_checkpoint_token(const std::string& html);
    Json::Value parse_json_from_html(const std::string& html);
};
