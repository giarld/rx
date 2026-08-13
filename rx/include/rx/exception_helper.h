#ifndef RX_EXCEPTION_HELPER_H
#define RX_EXCEPTION_HELPER_H

#include <gx/gany.h>

#include <exception>


namespace rx
{
class ExceptionHelper
{
public:
    static GAnyException fromCurrentException(const char *fallbackMessage)
    {
        try {
            throw;
        } catch (const GAnyException &e) {
            return e;
        } catch (const std::exception &e) {
            return GAnyException(e.what());
        } catch (...) {
            return GAnyException(fallbackMessage);
        }
    }
};
} // rx

#endif // RX_EXCEPTION_HELPER_H
