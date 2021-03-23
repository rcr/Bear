/*  Copyright (C) 2012-2021 by László Nagy
    This file is part of Bear.

    Bear is a tool to generate compilation database for clang tooling.

    Bear is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Bear is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "collect/SessionLibrary.h"

#include "intercept/Flags.h"
#include "libsys/Errors.h"
#include "libsys/Path.h"
#include "libsys/Process.h"
#include "report/libexec/Environments.h"
#include "report/wrapper/Flags.h"

#include <spdlog/spdlog.h>

#include <functional>

namespace {
    constexpr char GLIBC_PRELOAD_KEY[] = "LD_PRELOAD";
}

namespace ic {

    rust::Result<Session::Ptr> LibraryPreloadSession::from(const flags::Arguments& args)
    {
        auto verbose = args.as_bool(flags::VERBOSE).unwrap_or(false);
        auto library = args.as_string(ic::LIBRARY);
        auto wrapper = args.as_string(ic::WRAPPER);

        return merge(library, wrapper)
            .map<Session::Ptr>([&verbose](auto tuple) {
                const auto& [library, wrapper] = tuple;
                return std::make_shared<LibraryPreloadSession>(verbose, library, wrapper);
            });
    }

    LibraryPreloadSession::LibraryPreloadSession(
        bool verbose,
        const std::string_view &library,
        const std::string_view &executor)
            : Session()
            , verbose_(verbose)
            , library_(library)
            , executor_(executor)
    {
        spdlog::debug("Created library preload session. [library={0}, executor={1}]", library_, executor_);
    }

    rust::Result<rpc::Execution> LibraryPreloadSession::resolve(const rpc::Execution &execution) const
    {
        spdlog::debug("trying to resolve for library: {}", execution.executable());

        rpc::Execution candidate(execution);
        update(*candidate.mutable_environment());

        return rust::Ok(candidate);
    }

    sys::Process::Builder LibraryPreloadSession::supervise(const ic::Execution &execution) const
    {
        auto builder = sys::Process::Builder(executor_)
                .add_argument(executor_)
                .add_argument(wr::DESTINATION)
                .add_argument(*session_locator_);

        if (verbose_) {
            builder.add_argument(wr::VERBOSE);
        }

        return builder
                .add_argument(wr::EXECUTE)
                .add_argument(execution.executable)
                .add_argument(wr::COMMAND)
                .add_arguments(execution.arguments.begin(), execution.arguments.end())
                .set_environment(update(execution.environment));
    }

    std::map<std::string, std::string>
    LibraryPreloadSession::update(const std::map<std::string, std::string> &env) const
    {
        std::map<std::string, std::string> copy(env);
        if (verbose_) {
            copy[el::env::KEY_VERBOSE] = "true";
        }
        copy[el::env::KEY_DESTINATION] = *session_locator_;
        copy[el::env::KEY_REPORTER] = executor_;
        copy[GLIBC_PRELOAD_KEY] = Session::keep_front_in_path(library_, copy[GLIBC_PRELOAD_KEY]);

        return copy;
    }

    void LibraryPreloadSession::update(google::protobuf::Map<std::string, std::string> &env) const {
        if (verbose_) {
            env[el::env::KEY_VERBOSE] = "true";
        }
        env[el::env::KEY_DESTINATION] = *session_locator_;
        env[el::env::KEY_REPORTER] = executor_;
        env[GLIBC_PRELOAD_KEY] = Session::keep_front_in_path(library_, env[GLIBC_PRELOAD_KEY]);
    }
}
