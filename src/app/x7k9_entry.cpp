#include "internal/x7k9_core.hpp"

#include <Windows.h>

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

constexpr const char* kR = "\033[0m";
constexpr const char* kB = "\033[1m";
constexpr const char* kD = "\033[2m";
constexpr const char* kCy = "\033[36m";
constexpr const char* kMg = "\033[35m";
constexpr const char* kYe = "\033[33m";
constexpr const char* kGn = "\033[32m";
constexpr const char* kRd = "\033[31m";
constexpr const char* kWh = "\033[97m";

void x7k9_console_colors() {
    auto enable_vt = [](DWORD handle_id) {
        HANDLE handle = GetStdHandle(handle_id);
        if (handle == INVALID_HANDLE_VALUE) {
            return;
        }
        DWORD mode = 0;
        if (GetConsoleMode(handle, &mode)) {
            SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    };
    enable_vt(STD_OUTPUT_HANDLE);
    enable_vt(STD_ERROR_HANDLE);
}

enum class X7Mode { Login, LogAndActorDetails };

struct X7Opts {
    std::string username;
    std::string password;
    std::string server;
    X7Mode mode = X7Mode::LogAndActorDetails;
};

void x7k9_usage() {
    std::cout << kB << kCy << "mspc" << kR << kWh << " made by " << kB << kCy << "what" << kR << kWh
              << " | discord : " << kB << kMg << "aq2o" << kR << kWh
              << " | github : " << kB << kCy << "https://github.com/whattgit" << kR << "\n\n";

    std::cout << kB << kYe << "usage:" << kR << "\n";
    std::cout << "  " << kGn << "mspc" << kR << " " << kB << kGn << "--login" << kR << " "
              << kGn << "--username" << kR << " " << kD << "<user>" << kR << " "
              << kGn << "--password" << kR << " " << kD << "<pass>" << kR << " "
              << kGn << "--server" << kR << " " << kD << "<FR|UK|...>" << kR << "\n";
    std::cout << "  " << kGn << "mspc" << kR << " " << kB << kGn << "--logandactordetails" << kR << " "
              << kGn << "--username" << kR << " " << kD << "<user>" << kR << " "
              << kGn << "--password" << kR << " " << kD << "<pass>" << kR << " "
              << kGn << "--server" << kR << " " << kD << "<FR|UK|...>" << kR << "\n\n";

    std::cout << kB << kYe << "modes:" << kR << "\n";
    std::cout << "  " << kB << kGn << "--login" << kR << "              "
              << kD << "ticket, access_token, profile_id" << kR << "\n";
    std::cout << "  " << kB << kGn << "--logandactordetails" << kR << " "
              << kD << "login + LoadActorDetailsExtended (compact json)" << kR << "\n";
}

std::string x7k9_req(int& index, int argc, char** argv, const std::string& flag) {
    if (index + 1 >= argc) {
        throw std::runtime_error("missing value for " + flag);
    }
    ++index;
    return argv[index];
}

bool x7k9_is_flag(const std::string& arg, const char* long_name, const char* short_name) {
    return arg == long_name || arg == short_name;
}

X7Opts x7k9_parse(int argc, char** argv) {
    X7Opts options;
    bool mode_set = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--login") {
            options.mode = X7Mode::Login;
            mode_set = true;
        } else if (arg == "--logandactordetails") {
            options.mode = X7Mode::LogAndActorDetails;
            mode_set = true;
        } else if (x7k9_is_flag(arg, "--username", "-username") || arg == "-u") {
            options.username = x7k9_req(i, argc, argv, arg);
        } else if (x7k9_is_flag(arg, "--password", "-password") || arg == "-p") {
            options.password = x7k9_req(i, argc, argv, arg);
        } else if (x7k9_is_flag(arg, "--server", "-server") || arg == "-s") {
            options.server = x7k9_req(i, argc, argv, arg);
        } else if (arg == "-h" || arg == "--help") {
            x7k9_usage();
            std::exit(0);
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }
    if (!mode_set) {
        throw std::runtime_error("mode required: --login or --logandactordetails");
    }
    if (options.username.empty() || options.password.empty() || options.server.empty()) {
        throw std::runtime_error("--username, --password and --server are required");
    }
    return options;
}

struct X7Session {
    x7k2::q9m4::X7A1 auth;
    x7k2::q9m4::X7L2 login;
};

X7Session x7k9_session(const X7Opts& options) {
    X7Session session;
    session.auth = x7k2::q9m4::x7k9_w2q8(options.server, options.username, options.password);

    const x7k2::q9m4::X7R3 login = x7k2::q9m4::x7k9_p4r6(
        options.server,
        "MovieStarPlanet.WebService.User.AMFUserServiceWeb.Login",
        {
            x7k2::q9m4::X7V4::make_string(options.username),
            x7k2::q9m4::X7V4::make_string(options.password),
            x7k2::q9m4::X7V4::make_array({}),
            x7k2::q9m4::X7V4::make_null(),
            x7k2::q9m4::X7V4::make_null(),
            x7k2::q9m4::X7V4::make_string("MSP1-Standalone:XXXXXX"),
        });

    if (login.status != 200) {
        throw std::runtime_error(
            "login http " + std::to_string(login.status) +
            (login.raw_error.empty() ? "" : ": " + login.raw_error));
    }

    const auto parsed = x7k2::q9m4::x7k9_m3n5(login.json_body, login.raw_body);
    if (!parsed || parsed->status != "Success" || parsed->ticket.empty() || parsed->actor_id == 0) {
        throw std::runtime_error("login failed");
    }
    session.login = *parsed;
    return session;
}

}  // namespace

int main(int argc, char** argv) {
    x7k9_console_colors();
    try {
        if (argc <= 1) {
            x7k9_usage();
            return 1;
        }

        const X7Opts options = x7k9_parse(argc, argv);
        const X7Session session = x7k9_session(options);

        if (options.mode == X7Mode::Login) {
            std::cout << kGn << "ticket" << kR << '=' << session.login.ticket << '\n';
            std::cout << kGn << "access_token" << kR << '=' << session.auth.access_token << '\n';
            std::cout << kGn << "profile_id" << kR << '=' << session.auth.profile_id << '\n';
            x7k2::q9m4::x7k9_n2s0();
            return 0;
        }

        const x7k2::q9m4::X7R3 details = x7k2::q9m4::x7k9_p4r6(
            options.server,
            "MovieStarPlanet.WebService.UserSession.AMFUserSessionService.LoadActorDetailsExtended",
            {
                x7k2::q9m4::x7k9_t7h1(session.login.ticket, session.auth.access_token),
                x7k2::q9m4::X7V4::make_int(session.login.actor_id),
            });

        if (details.status != 200 || details.raw_error == "amf decode failed") {
            throw std::runtime_error(
                "LoadActorDetailsExtended failed" +
                (details.raw_error.empty() ? "" : ": " + details.raw_error));
        }
        if (details.json_body.empty() || details.json_body.front() != '{') {
            throw std::runtime_error("LoadActorDetailsExtended invalid amf response");
        }

        std::cout << details.json_body << '\n';
        x7k2::q9m4::x7k9_n2s0();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << kB << kRd << "error:" << kR << ' ' << ex.what() << '\n';
        x7k2::q9m4::x7k9_n2s0();
        return 1;
    }
}
