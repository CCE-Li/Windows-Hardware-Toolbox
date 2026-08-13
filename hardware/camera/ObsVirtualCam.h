#pragma once

#include <cstdint>
#include <string>

namespace htb {

class ObsVirtualCam {
public:
    ~ObsVirtualCam();

    bool open(uint32_t width, uint32_t height, uint64_t interval100ns);
    void writeFrame(const uint8_t* nv12, uint64_t timestamp100ns);
    void close();

    bool active() const;
    const std::string& lastError() const { return m_lastError; }

private:
    struct Impl;
    Impl* m_impl = nullptr;
    std::string m_lastError;
};

} // namespace htb
