#pragma once

#include <cassert>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace skyinput {

class Error {
public:
    Error(std::string operation, unsigned long code);

    static Error fromLastError(std::string operation);

    const std::string &operation() const { return m_operation; }
    unsigned long code() const { return m_code; }
    std::string message() const;

private:
    std::string m_operation;
    unsigned long m_code = 0;
};

template <typename T>
class [[nodiscard]] Result {
public:
    Result(T value) : m_state(std::in_place_index<0>, std::move(value)) {}
    Result(Error error) : m_state(std::in_place_index<1>, std::move(error)) {}

    bool ok() const noexcept { return m_state.index() == 0; }
    explicit operator bool() const noexcept { return ok(); }

    T &value() &
    {
        assert(ok());
        return std::get<0>(m_state);
    }

    const T &value() const &
    {
        assert(ok());
        return std::get<0>(m_state);
    }

    T &&value() &&
    {
        assert(ok());
        return std::get<0>(std::move(m_state));
    }

    const Error &error() const
    {
        assert(!ok());
        return std::get<1>(m_state);
    }

private:
    std::variant<T, Error> m_state;
};

template <>
class [[nodiscard]] Result<void> {
public:
    Result() = default;
    Result(Error error) : m_error(std::move(error)) {}

    bool ok() const noexcept { return !m_error.has_value(); }
    explicit operator bool() const noexcept { return ok(); }

    const Error &error() const
    {
        assert(!ok());
        return *m_error;
    }

private:
    std::optional<Error> m_error;
};

}  // namespace skyinput
