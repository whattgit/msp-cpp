#include "internal/x7k9_core.hpp"

#include <cstdlib>

namespace x7k2::q9m4 {
namespace {

X7V4 make_ticket_object(const std::string& ticket, const std::string& access_token) {
    X7O5 object;
    object.fields["Ticket"] = X7V4::make_string(ticket);
    if (access_token.empty()) {
        object.fields["anyAttribute"] = X7V4::make_null();
    } else {
        X7O5 token_object;
        token_object.fields["Token"] = X7V4::make_string(access_token);
        object.fields["anyAttribute"] = X7V4::make_object(std::move(token_object));
    }
    return X7V4::make_object(std::move(object));
}

}  // namespace

X7V4 x7k9_t7h1(const std::string& ticket, const std::string& access_token) {
    const int marking = static_cast<int>(std::rand() % 999999) + 1;
    const std::string counter = std::to_string(marking);
    const std::string marked = ticket + md5_hex(counter) + bytes_to_hex(std::vector<uint8_t>(counter.begin(), counter.end()));
    return make_ticket_object(marked, access_token);
}

}  // namespace x7k2::q9m4
