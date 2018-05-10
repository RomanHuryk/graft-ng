#pragma once

#include <forward_list>
#include <functional>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <iostream>

#include "r3.h"
#include "inout.h"
#include "context.h"

namespace graft {

template<typename In, typename Out>
class RouterT
{
public:
    enum class Status
    {
        Ok,
        Forward,
        Error,
        Drop,
        None,
    };

public:
    using vars_t = std::vector<std::pair<std::string, std::string>>;
    using Handler = std::function<Status (const vars_t&, const In&, Context&, Out& ) >;

public:
    struct Handler3
    {
        Handler3() = default;

        Handler3(const Handler3&) = default;
        Handler3(Handler3&&) = default;
        Handler3& operator = (const Handler3&) = default;
        Handler3& operator = (Handler3&&) = default;
        ~Handler3() = default;

        Handler3(const Handler& pre_action, const Handler& action, const Handler& post_action)
            : pre_action(pre_action), worker_action(action), post_action(post_action)
        { }
        Handler3(Handler&& pre_action, Handler&& action, Handler&& post_action)
            : pre_action(std::move(pre_action)), worker_action(std::move(action)), post_action(std::move(post_action))
        { }

        Handler3(const Handler& worker_action) : worker_action(worker_action) { }
        Handler3(Handler&& worker_action) : worker_action(std::move(worker_action)) { }
    public:
        Handler pre_action;
        Handler worker_action;
        Handler post_action;
    };

public:
    RouterT()
    {
        if (!m_node)
            m_node = r3_tree_create(10);
    }

    ~RouterT()
    {
        if (m_node)
        {
            r3_tree_free(m_node);
            m_node = NULL;
        }
    }

    void addRoute(const std::string& endpoint, int methods, const Handler3& ph3)
    {
        addRoute(endpoint, methods, Handler3(ph3));
    }

    void addRoute(const std::string& endpoint, int methods, Handler3&& ph3)
    {
        m_routes.push_front({endpoint, methods, std::move(ph3)});
    }

    bool arm()
    {
        std::for_each(m_routes.begin(), m_routes.end(),
                      [this](Route& r)
        {
            r3_tree_insert_route(
                        m_node,
                        r.methods,
                        r.endpoint.c_str(),
                        &r
                        );
        });

        char *errstr = NULL;
        int err = r3_tree_compile(m_node, &errstr);

        if (err != 0)
            std::cout << "error: " << std::string(errstr) << std::endl;
        else
            r3_tree_dump(m_node, 0);

        return (err == 0);
    }

    struct JobParams
    {
        Input input;
        vars_t vars;
        Handler3 h3;
    };

    bool match(const std::string& target, int method, JobParams& params) const
    {
        match_entry *entry;
        R3Route *m;
        bool ret = false;

        entry = match_entry_create(target.c_str());
        entry->request_method = method;

        m = r3_tree_match_route(m_node, entry);
        if (m)
        {
            for (size_t i = 0; i < entry->vars.tokens.size; i++)
                params.vars.emplace_back(std::make_pair(
                                             std::move(std::string(entry->vars.slugs.entries[i].base, entry->vars.slugs.entries[i].len)),
                                             std::move(std::string(entry->vars.tokens.entries[i].base, entry->vars.tokens.entries[i].len))
                                             ));

            params.h3 = static_cast<Route*>(m->data)->h3;
            ret = true;
        }
        match_entry_free(entry);

        return ret;
    }

private:
    struct Route
    {
        std::string endpoint;
        int methods;
        Handler3 h3;
    };

    std::forward_list<Route> m_routes;

    static R3Node *m_node;
};

template<typename In, typename Out>
R3Node *RouterT<In, Out>::m_node = nullptr;

}//namespace graft
