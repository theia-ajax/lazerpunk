#pragma once

#include <any>
#include <string>
#include <format>
#include <functional>
#include <map>
#include <ranges>
#include <typeindex>
#include <unordered_map>

#include "draw.h"

struct DrawContext;

namespace debug
{
	namespace internal
	{
		void WriteWatch(const std::string& message);
		void WriteLog(const std::string& message);
	}

	template <typename ...Args>
	void Watch(std::string_view fmt, Args&&... args)
	{
		internal::WriteWatch(std::vformat(fmt, std::make_format_args(std::forward<Args>(args)...)));
	}

	template <typename ...Args>
	void Log(std::string_view fmt, Args&&... args)
	{
		internal::WriteLog(std::vformat(fmt, std::make_format_args(std::forward<Args>(args)...)));
	}

	void LogWriteToFile(const char* fileName);

	void DrawWatch(const DrawContext& ctx, Color color);

	struct DevConsoleConfig
	{
		int canvasX{}, canvasY{};
		TTF_Font* font = nullptr;
		SDL_Renderer* renderer = nullptr;
		void* commandContext = nullptr;
		int historyLength = 50;
	};

	void InitDevConsole(const DevConsoleConfig& config);
	void ToggleDevConsole();
	bool IsConsoleVisible();
	void DevConsoleKeyInput(int32_t key, bool press, bool repeat);
	void DevConsoleTextInput(char text[32]);
	void DevConsoleTextEdit(char text[32], int32_t start, int32_t length);
	void DevConsoleClear();
	void DevConsoleExecute(const std::string& commandStr);
	void DrawConsole(const DrawContext& ctx, float dt);

	struct ICaller {
		virtual ~ICaller() = default;
		virtual std::any call(std::vector<std::any>& args) = 0;
		virtual std::any callParse(const std::vector<std::string>& args) = 0;
	};

	template<int N, typename... Ts> using type_n_t = std::tuple_element_t<N, std::tuple<Ts...>>;

	template<class T, class F>
	auto parse_string_to_any(F const& f) -> std::pair<const std::type_index, std::function<std::any(std::string)>>
	{
		return {
			std::type_index(typeid(T)),
			[g = f](std::string s)
			{
				return std::make_any<T>(g(s));
			}
		};
	}

	static std::unordered_map parseAnyFuncs
	{
		parse_string_to_any<int>([](std::string s) { return (int)strtol(s.c_str(), nullptr, 10); }),
		parse_string_to_any<bool>([](std::string s) { return !s.empty() && (s[0] == 't' || s[0] == 'T' || s[0] == '1'); }),
		parse_string_to_any<std::string>([](std::string s) { return s; }),
	};

	template <class T>
	T parse_any(const std::string& s)
	{
		if (std::type_index ti(typeid(T)); parseAnyFuncs.contains(ti))
			return std::any_cast<T>(parseAnyFuncs[ti](std::any_cast<std::string>(s)));
		return std::any_cast<T>(s);
	}

	template<typename R, typename... A>
	struct Caller final : ICaller {
		template <size_t... Is>
		auto make_tuple_impl(const std::vector<std::any>& anyArgs, std::index_sequence<Is...>)
		{
			return std::make_tuple(std::any_cast<std::decay_t<decltype(std::get<Is>(args))>>(anyArgs.at(Is))...);
		}

		template <size_t N>
		auto make_tuple(const std::vector<std::any>& anyArgs) {
			return make_tuple_impl(anyArgs, std::make_index_sequence<N>{});
		}

		std::any call(std::vector<std::any>& anyArgs) override {
			args = make_tuple<sizeof...(A)>(anyArgs);
			ret = std::apply(func, args);
			return { ret };
		}

		template <size_t... Is>
		auto make_tuple_parse_impl(const std::vector<std::string>& anyArgs, std::index_sequence<Is...>) {
			return std::make_tuple(parse_any<std::decay_t<type_n_t<Is, A...>>>(anyArgs.at(Is))...);
		}

		template <size_t N>
		auto make_tuple_parse(const std::vector<std::string>& anyArgs) {
			return make_tuple_parse_impl(anyArgs, std::make_index_sequence<N>{});
		};

		std::any callParse(const std::vector<std::string>& anyArgs) override
		{
			args = make_tuple_parse<sizeof...(A)>(anyArgs);
			ret = std::apply(func, args);
			return { ret };
		}

		Caller(std::function<R(A...)>& func_)
			: func(func_)
		{}

		std::function<R(A...)>& func;
		std::tuple<A...> args;
		R ret;
	};

	struct GenericCallback {

		template <class R, class... A>
		GenericCallback& operator=(std::function<R(A...)>&& func_) {
			func = std::move(func_);
			caller = std::make_unique<Caller<R, A...>>(std::any_cast<std::function<R(A...)>&>(func));
			return *this;
		}

		template <class Func>
		GenericCallback& operator=(Func&& func_) {
			return (*this = std::function(std::forward<Func>(func_)));
		}

		std::any callAny(std::vector<std::any>& args) const
		{
			return caller->call(args);
		}

		std::any callParse(const std::vector<std::string>& args) const
		{
			return caller->callParse(args);
		}

		template <class R, class... Args>
		R call(Args&&... args) {
			auto& f = std::any_cast<std::function<R(Args...)>&>(func);
			return f(std::forward<Args>(args)...);
		}

		template <class R, class... Args>
		void operator()(Args&&... args)
		{
			return call<R, Args...>(std::forward<Args>(args)...);
		}

		std::any func;
		std::unique_ptr<ICaller> caller;
	};

	inline std::string _Log(std::string message) { Log(message); return message; }

	class CommandProcessor
	{
	public:
		CommandProcessor()
			: callbacks()
		{
			AddCommand("clear", []() -> int { DevConsoleClear(); return 0; }, "Clears console output.");
			AddCommand("print", [](std::string m) { Log(m); return 0; }, R"(Prints {:s} to console.)");
			AddCommand("help", [this] { this->HelpCommand(); return 0; }, R"(Lists available commands.)");
		}

		template <class Func>
		void AddCommand(const std::string& name, Func&& func, const std::string& description = "")
		{
			callbacks[name] = func;
			commands[name] = CommandEntry{ StrId(name), description };
		}

		void HelpCommand();

		bool HasCommand(const std::string& name) const { return callbacks.contains(name); }
		bool HasCommand(StrId nameId) const { return callbacks.contains(nameId); }
		const GenericCallback& GetCommand(const std::string& name) const { return callbacks.at(name); }
		const GenericCallback& GetCommand(StrId nameId) const { return callbacks.at(nameId); }
		void ParseCommandArgs(const std::string& commandStr, StrId& commandId, std::vector<std::string>& params) const;

	private:
		struct CommandEntry
		{
			StrId commandId;
			std::string description;
		};
		std::unordered_map<StrId, GenericCallback> callbacks;
		std::unordered_map<StrId, CommandEntry> commands;
	};

	CommandProcessor& DevConsoleGetCommandProcessor();

	template <class Func>
	void DevConsoleAddCommand(const char* name, Func&& func)
	{
		DevConsoleGetCommandProcessor().AddCommand(name, func);
	}
}
