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

void check(BIYOS_env *env, BIYOS_status status)
{
    if (status == BIYOS_OK)
    {
        return;
    }
    BIYOS_error error{};
    error.size = sizeof error;
    if (BIYOS_lasterror(env, &error) != BIYOS_OK || error.msg == nullptr || *error.msg == '\0')
    {
        throw std::runtime_error("status " + std::to_string(status));
    }
    throw std::runtime_error(error.msg);
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

ModelPtr read_model(BIYOS_env *env, const char *path)
{
    BIYOS_model *model = nullptr;
    check(env, BIYOS_readmdl(env, path, BIYOS_FILEFMT_MPS, &model));
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

int round_trip(const char *source, const char *scratch)
{
    const EnvPtr env = new_env();
    Stopwatch watch;
    const ModelPtr model = read_model(env.get(), source);
    watch.lap("read");
    const BIYOS_int format = BIYOS_FILEFMT_MPS;
    check(env.get(), BIYOS_writemdl(model.get(), scratch, format));
    watch.lap("write");
    const ModelPtr back = read_model(env.get(), scratch);
    watch.lap("reread");
    const std::optional<std::string> found = difference(env.get(), model.get(), back.get());
    if (!found)
    {
        return kSame;
    }
    std::cout << *found << '\n';
    return kDiffers;
}

}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cerr << "usage: roundtrip <file> <scratch>\n";
        return kError;
    }
    try
    {
        return round_trip(argv[1], argv[2]);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return kError;
    }
}
