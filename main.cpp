#include "instagram.hpp"
#include <iostream>

int main() {
    Instagram ig;

    try {
        // Get public profile
        Profile profile = ig.get_profile("instagram");
        std::cout << "Username: " << profile.username << "\n";
        std::cout << "Followers: " << profile.followers_count << "\n";
    } catch (const InstagramException& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
