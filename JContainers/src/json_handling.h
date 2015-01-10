#pragma once

#include <set>
#include <vector>
#include <map>
#include <jansson.h>
#include <memory>

#include "collections.h"
#include "form_handling.h"
#include "boost_extras.h"
#include "path_resolving.h"

namespace collections {

    inline std::unique_ptr<FILE, decltype(&fclose)> make_unique_file(FILE *file) {
        return std::unique_ptr<FILE, decltype(&fclose)> (file, &fclose);
    }

    template<class T, class D>
    inline std::unique_ptr<T, D> make_unique_ptr(T* data, D destr) {
        return std::unique_ptr<T, D> (data, destr);
    }

    template<class T>
    inline std::unique_ptr<T, std::default_delete<T> > make_unique_ptr(T* data) {
        return std::unique_ptr<T, std::default_delete<T> > ( data, std::default_delete<T>() );
    }

    typedef struct json_t * json_ref;
    typedef decltype(make_unique_ptr((json_t*)nullptr, json_decref)) json_unique_ref;

    namespace reference_serialization {
        // In current state it does not YET requires UTF-8 support

        const char prefix[] = "__reference|";
        const char separator = '|';

        inline bool is_special_string(const char *str) {
            char spec[] = "__";
            return str && strncmp(str, spec, sizeof spec - 1) == 0;
        }

        inline bool is_reference(const char *str) {
            return str && strncmp(str, prefix, sizeof prefix - 1) == 0;
        }

        inline const char* extract_path(const char *str) {
            return is_reference(str) ? (str + sizeof(prefix) - 1) : nullptr;
        }
    }

    class json_deserializer {
        typedef std::vector<std::pair<object_base*, json_ref> > objects_to_fill;

        typedef boost::variant<size_t, std::string, FormId> key_variant;
        // path - <container, key> pairs relationship
        typedef std::map<std::string, std::vector<std::pair<object_base*, key_variant > > > key_info_map;

        tes_context& _context;
        objects_to_fill _toFill;
        key_info_map _toResolve;

        explicit json_deserializer(tes_context& context) : _context(context) {}

    public:

        static json_unique_ref json_from_file(const char *path) {
            json_error_t error; //  TODO: output error
            json_ref ref = json_load_file(path, 0, &error);
            return make_unique_ptr(ref, json_decref);
        }

        static json_unique_ref json_from_data(const char *data) {
            json_error_t error; //  TODO: output error
            json_ref ref = json_loads(data, 0, &error);
            return make_unique_ptr(ref, json_decref);
        }

        static object_base* object_from_json_data(tes_context& context, const char *data) {
            auto json = json_from_data(data);
            return json_deserializer(context)._object_from_json( json.get() );
        }

        static object_base* object_from_json(tes_context& context, json_ref ref) {
            return json_deserializer(context)._object_from_json( ref );
        }

        static object_base* object_from_file(tes_context& context, const char *path) {
            auto json = json_from_file(path);
            return json_deserializer(context)._object_from_json( json.get() );
        }

    private:

        object_base* _object_from_json(json_ref ref) {
            if (!ref) {
                return nullptr;
            }

            auto& root = make_placeholder(ref);

            while (_toFill.empty() == false) {
                objects_to_fill toFill;
                toFill.swap(_toFill);

                for (auto& pair : toFill) {
                    fill_object(*pair.first, pair.second);
                }
            }

            resolve_references(root);

            return &root;
        }

        void resolve_references(object_base& root) {

            for (const auto& pair : _toResolve) {
                auto& path = pair.first;
                object_base *resolvedObject = nullptr;

                path_resolving::resolve(_context, &root, path.c_str(), [&resolvedObject](Item *itm) {
                    if (itm) {
                        resolvedObject = itm->object();
                    }
                });

                if (!resolvedObject) {
                    continue;
                }

                for (auto& obj2Key : pair.second) {
                    struct value_setter : boost::static_visitor < > {
                        object_base& obj;
                        object_base* val;

                        value_setter(object_base& o, object_base* value) : obj(o), val(value) {}

                        void operator()(const std::string & key) const {
                            obj.as<map>()->setValueForKey(key, Item(val));
                        }

                        void operator()(const size_t& key) const {
                            obj.as<array>()->setItem(key, Item(val));
                        }

                        void operator()(const FormId& key) const {
                            obj.as<form_map>()->setValueForKey(key, Item(val));
                        }
                    };

                    boost::apply_visitor(
                        value_setter(*obj2Key.first, resolvedObject),
                        obj2Key.second);
                }
            }

        }

        void fill_object(object_base& object, json_ref val) {
            object_lock lock(object);

            if (array *arr = object.as<array>()) {
                /* array is a JSON array */
                size_t index = 0;
                json_t *value = nullptr;

                json_array_foreach(val, index, value) {
                    arr->u_push(make_item(value, object, index));
                }
            }
            else if (map *cnt = object.as<map>()) {
                const char *key;
                json_t *value;

                json_object_foreach(val, key, value) {
                    cnt->u_setValueForKey(key, make_item(value, object, key));
                }
            }
            else if (form_map *cnt = object.as<form_map>()) {
                const char *key;
                json_t *value;

                json_object_foreach(val, key, value) {
                    auto fkey = form_handling::from_string(key);
                    if (fkey) {
                        cnt->u_setValueForKey(*fkey, make_item(value, object, *fkey));
                    }
                }
            }
            else
                assert(false);
        }

        object_base& make_placeholder(json_ref val) {
            object_base *object = nullptr;
            auto type = json_typeof(val);

            if (type == JSON_ARRAY) {
                object = &array::object(_context);
            }
            else if (type == JSON_OBJECT) {
                if (!json_object_get(val, form_handling::kFormData)) {
                    object = &map::object(_context);
                } else {
                    object = &form_map::object(_context);
                }
            }

            jc_assert(object);
            _toFill.push_back(std::make_pair(object, val));
            return *object;
        }

        template<class K>
        bool schedule_ref_resolving(const char *ref_string, object_base& container, const K& item_key) {
            const char* path = reference_serialization::extract_path(ref_string);

            if (!path) {
                return false;
            }

            _toResolve[path].push_back( std::make_pair(&container, item_key) );
            return true;
        }

        template<class K>
        Item make_item(json_ref val, object_base& container, const K& item_key) {
            Item item;

            switch (json_typeof(val))
            {
            case JSON_OBJECT:
            case JSON_ARRAY:
                item = &make_placeholder(val);
                break;
            case JSON_STRING:{
                auto string = json_string_value(val);

                if (!reference_serialization::is_special_string(string)) {
                    item = string;
                } else {
                    if (form_handling::is_form_string(string)) {
                        /*  having dilemma here:
                            if string looks likes form-string and plugin name can't be resolved:
                            a. lost info and convert it to FormZero
                            b. save info and convert it to string
                        */
                        item = form_handling::from_string(string).get_value_or(FormZero);
                    }
                    else if (schedule_ref_resolving(string, container, item_key)) { // otherwise it's reference string?
                        ;
                    }
                    else {  // otherwise it's just a string, althought it starts with "__"
                        item = string;
                    }
                }

            }
                break;
            case JSON_INTEGER:
                item = (int)json_integer_value(val);
                break;
            case JSON_REAL:
                item = json_real_value(val);
                break;
            case JSON_TRUE:
            case JSON_FALSE:
                item = json_is_true(val);
                break;
            case JSON_NULL:
            default:
                break;
            }

            return item;
        }
    };


    class json_serializer {

        typedef std::set<object_base*> collection_set;
        typedef std::vector<std::pair<object_base*, json_ref> > objects_to_fill;

        const object_base& _root;
        collection_set _serializedObjects;
        objects_to_fill _toFill;

        // contained - <container, key> relationship
        typedef boost::variant<size_t, std::string, FormId> key_variant;
        typedef std::map<const object_base*, std::pair<const object_base*, key_variant > > key_info_map;
        key_info_map _keyInfo;

        explicit json_serializer(const object_base& root) : _root(root) {}

    public:

        static json_unique_ref create_json_value(object_base &root) {
            return make_unique_ptr( json_serializer(root)._write_json(root), &json_decref);
        }

        static auto create_json_data(object_base &root) -> decltype(make_unique_ptr((char*)nullptr, free)) {

            auto jvalue = create_json_value(root);

            return make_unique_ptr(
                jvalue ? json_dumps(jvalue.get(), JSON_INDENT(2)) : nullptr,
                free
            );
        }

    private:

        // writes to json
        json_ref _write_json(object_base &root) {

            auto root_value = create_placeholder(root);

            while (_toFill.empty() == false) {

                objects_to_fill toFill;
                toFill.swap(_toFill);

                for (auto& pair : toFill) {
                    fill_json_object(*pair.first, pair.second);
                }
            }

            return root_value;
        }

        json_ref create_placeholder(object_base& object) {

            json_ref placeholder = nullptr;

            if (object.as<array>()) {
                placeholder = json_array();
            }
            else if (object.as<map>() || object.as<form_map>()) {
                placeholder = json_object();
            }
            jc_assert(placeholder);

            _toFill.push_back( objects_to_fill::value_type(&object, placeholder) );
            _serializedObjects.insert(&object);

            return placeholder;
        }

        void fill_json_object(object_base& cnt, json_ref object) {

            object_lock lock(cnt);

            if (cnt.as<array>()) {
                size_t index = 0;
                for (auto& itm : cnt.as<array>()->u_container()) {
                    fill_key_info(itm, cnt, index++);
                    json_array_append_new(object, create_value(itm));
                }
            }
            else if (cnt.as<map>()) {
                for (auto& pair : cnt.as<map>()->u_container()) {
                    fill_key_info(pair.second, cnt, pair.first);
                    json_object_set_new(object, pair.first.c_str(), create_value(pair.second));
                }
            }
            else if (cnt.as<form_map>()) {
                // mark object as form_map container
                json_object_set_new(object, form_handling::kFormData, json_null());

                for (auto& pair : cnt.as<form_map>()->u_container()) {
                    auto key = form_handling::to_string(pair.first);
                    if (key) {
                        fill_key_info(pair.second, cnt, pair.first);
                        json_object_set_new(object, (*key).c_str(), create_value(pair.second));
                    }
                }
            }
        }

        template<class Key>
        void fill_key_info(const Item& itm, const object_base& in_object, const Key& key) {
            if (auto obj = itm.object()) {
                if (_keyInfo.find(obj) == _keyInfo.end()) {
                    _keyInfo.insert(key_info_map::value_type(obj, std::make_pair(&in_object, key)));
                }
            }
        }

        json_ref create_value(const Item& item) {

            struct item_visitor : boost::static_visitor<json_ref> {

                json_serializer& ser;

                item_visitor(json_serializer& s) : ser(s) {}

                static json_ref null() {
                    return json_null();
                }

                json_ref operator()(const std::string & val) const {
                    return json_string(val.c_str());
                }

                json_ref operator()(const boost::blank&) const {
                    return null();
                }

                json_ref operator()(const SInt32 & val) const {
                    return json_integer(val);
                }

                json_ref operator()(const Item::Real & val) const {
                    return json_real(val);
                }

                json_ref operator()(const FormId&  val) const {
                    auto formStr = form_handling::to_string(val);
                    if (formStr) {
                        return (*this)(*formStr);
                    }
                    else {
                        return null();
                    }
                }

                json_ref operator()(const internal_object_ref & val) const {
                    object_base *obj = val.get();

                    if (!obj) {
                        return null();
                    }

                    if (ser._serializedObjects.find(obj) != ser._serializedObjects.end()) {
                        return json_string(ser.path_to_object(*obj).c_str());
                    }

                    json_ref node = ser.create_placeholder(*obj);
                    return node;
                }

            } item_visitor = { *this };

            json_ref val = item.var().apply_visitor(item_visitor);
            return val;
        }

        std::string path_to_object(const object_base& obj) const {

            std::string path;

            struct path_appender : boost::static_visitor<> {
                std::string& p;

                path_appender(std::string& path) : p(path) {}

                void operator()(const std::string & key) const {
                    p.append(".");
                    p.append(key);
                }

                void operator()(const size_t& idx) const {
                    char data[20] = { 0 };
                    sprintf_s(data, "[%lu]", idx);
                    p.append(data);
                }

                void operator()(const FormId& fid) const {
                    p.append("[");
                    p.append(*form_handling::to_string(fid));
                    p.append("]");
                }

            } path_appender = {path};

            std::deque<const key_variant*> keys;
            const object_base *child = &obj;

            while (child != &_root) {

                auto itr = _keyInfo.find(child);

                if (itr != _keyInfo.end()) {
                    auto& pair = itr->second;
                    child = pair.first;
                    keys.push_front(&pair.second);
                }
                else {
                    break;
                }
            }

            path.append(reference_serialization::prefix);

            for (auto& key_var : keys) {
                boost::apply_visitor(path_appender, *key_var);
            }

            return path;
        }

    };

}


