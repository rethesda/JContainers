#pragma once

#include <functional>
#include <boost/optional.hpp>

namespace collections
{
    class item;
    class object_base;
    class tes_context;

    namespace path_resolving {

        void resolve(tes_context& ctx, item& target, const char *cpath, std::function<void(item *)> itemFunction, bool createMissingKeys = false);

        void resolve(tes_context& ctx, object_base *target, const char *cpath, std::function<void(item *)> itemFunction, bool createMissingKeys = false);

        template<class T>
        inline T _resolve(tes_context& ctx, object_base *target, const char *cpath, T def = T(0)) {
            resolve(ctx, target, cpath, [&](item *itm) {
                if (itm) {
                    def = itm->readAs<T>();
                }
            });

            return def;
        }
    }

    namespace path_resolving_new {

        void resolve_(tes_context& ctx, item& target, const char *cpath, std::function<void(item *)> itemFunction, bool create_missing_keys = false);
        void resolve(tes_context& ctx, object_base& target, const char *cpath, std::function<void(item *)> itemFunction, bool create_missing_keys = false);
        boost::optional<item> resolve(tes_context& ctx, object_base& target, const char *cpath);

        template<class T>
        bool assign(tes_context& ctx, object_base& target, const char *cpath, T&& value) {

        }
    }
}
