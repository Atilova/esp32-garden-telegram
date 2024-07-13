#pragma once

#include <stdio.h>
#include <IPAddress.h>

#define SET_VAR(var) #var

#define FROM_ENV(var) SET_VAR(var)
#define NUMBER_FROM_ENV(var) numberFromEnv(SET_VAR(var))
#define IP_FROM_ENV(var) ipAddressFromString(SET_VAR(var))

int64_t numberFromEnv(const char* number) {
    return std::atoll(number);
}

IPAddress ipAddressFromString(const char* address) {
    IPAddress ip;
    ip.fromString(address);
    return ip;
}
