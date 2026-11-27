#include <biyos/biyos.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace
{

constexpr int kSame = 0;
constexpr int kDiffers = 1;
constexpr int kError = 2;
constexpr int kUnwritable = 3;

using Clock = std::chrono::steady_clock;

struct EnvFree
{
    void operator()(BIYOS_env *env) const { BIYOS_freeenv(env); }
};

struct ModelFree
{
    void operator()(BIYOS_model *model) const { BIYOS_freemdl(model); }
};

using EnvPtr = std::unique_ptr<BIYOS_env, EnvFree>;
using ModelPtr = std::unique_ptr<BIYOS_model, ModelFree>;

class Stopwatch
{
  public:
    void lap(const char *phase)
    {
        const Clock::time_point now = Clock::now();
        const std::chrono::duration<double> seconds = now - start_;
        std::cout << phase << ' ' << seconds.count() << '\n';
        start_ = now;
    }

  private:
    Clock::time_point start_ = Clock::now();
};

std::string message_of(BIYOS_env *env, BIYOS_status status)
{
    BIYOS_error error{};
    error.size = sizeof error;
    if (BIYOS_lasterror(env, &error) != BIYOS_OK || error.msg == nullptr || *error.msg == '\0')
    {
        return "status " + std::to_string(status);
    }
    return error.msg;
}

void check(BIYOS_env *env, BIYOS_status status)
{
    if (status != BIYOS_OK)
    {
        throw std::runtime_error(message_of(env, status));
    }
}

EnvPtr new_env()
{
    BIYOS_env *env = nullptr;
    if (BIYOS_newenv(&env) != BIYOS_OK)
    {
        throw std::runtime_error("no environment");
    }
    return EnvPtr(env);
}

ModelPtr read_model(BIYOS_env *env, const char *path, BIYOS_int format)
{
    BIYOS_model *model = nullptr;
    check(env, BIYOS_readmdl(env, path, format, &model));
    return ModelPtr(model);
}

void keep_text(const BIYOS_event *event, void *data)
{
    if (std::strcmp(BIYOS_getevtmod(event), "model") == 0)
    {
        *static_cast<std::string *>(data) = BIYOS_getevttext(event);
    }
}

std::optional<std::string> difference(BIYOS_env *env, const BIYOS_model *a, const BIYOS_model *b)
{
    std::string text;
    BIYOS_index hook = 0;
    const BIYOS_int kind = BIYOS_EVTKND_MSG;
    const BIYOS_int level = BIYOS_EVTLVL_INFO;
    check(env, BIYOS_addenvhook(env, keep_text, &text, kind, level, &hook));
    BIYOS_int same = 0;
    const BIYOS_status status = BIYOS_cmpmdl(a, b, &same);
    BIYOS_delenvhook(env, hook);
    check(env, status);
    if (same)
    {
        return std::nullopt;
    }
    return text;
}

BIYOS_int written_as(BIYOS_int format)
{
    return format == BIYOS_FILEFMT_FIXMPS ? BIYOS_FILEFMT_MPS : format;
}

int round_trip(const char *source, const char *scratch, BIYOS_int given)
{
    const EnvPtr env = new_env();
    Stopwatch watch;
    const ModelPtr model = read_model(env.get(), source, given);
    watch.lap("read");
    const BIYOS_int format = written_as(given);
    const BIYOS_status written = BIYOS_writemdl(model.get(), scratch, format);
    if (written == BIYOS_ERR_UNSUPPORTED)
    {
        std::cout << message_of(env.get(), written) << '\n';
        return kUnwritable;
    }
    check(env.get(), written);
    watch.lap("write");
    const ModelPtr back = read_model(env.get(), scratch, format);
    watch.lap("reread");
    const std::optional<std::string> found = difference(env.get(), model.get(), back.get());
    if (!found)
    {
        return kSame;
    }
    std::cout << *found << '\n';
    return kDiffers;
}

std::optional<BIYOS_int> format_of(int argc, char **argv)
{
    if (argc == 3)
    {
        return BIYOS_FILEFMT_MPS;
    }
    if (argc == 4 && std::strcmp(argv[3], "fixed") == 0)
    {
        return BIYOS_FILEFMT_FIXMPS;
    }
    if (argc == 4 && std::strcmp(argv[3], "nl") == 0)
    {
        return BIYOS_FILEFMT_NL;
    }
    return std::nullopt;
}

}

int main(int argc, char **argv)
{
    const std::optional<BIYOS_int> format = format_of(argc, argv);
    if (!format)
    {
        std::cerr << "usage: roundtrip <file> <scratch> [fixed|nl]\n";
        return kError;
    }
    try
    {
        return round_trip(argv[1], argv[2], *format);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return kError;
    }
}
