#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <random>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <common/async.hpp>
#include <common/config_ini.hpp>

namespace test
{
	inline auto config_from(std::string_view text) -> config_ini
	{
		std::istringstream stream_v{ std::string(text) };
		return config_ini(stream_v);
	}

	inline auto bytes(std::string_view text) -> std::vector<std::byte>
	{
		std::vector<std::byte> result_v(text.size());
		for (std::size_t i = 0; i < text.size(); ++i)
			result_v[i] = (std::byte)text[i];
		return result_v;
	}

	inline auto bytes(std::initializer_list<int> values) -> std::vector<std::byte>
	{
		std::vector<std::byte> result_v;
		for (auto value : values)
			result_v.push_back((std::byte)value);
		return result_v;
	}

	inline auto pattern(std::size_t size) -> std::vector<std::byte>
	{
		std::vector<std::byte> result_v(size);
		for (std::size_t i = 0; i < size; ++i)
			result_v[i] = (std::byte)((i * 7u + i / 256u) & 0xffu);
		return result_v;
	}

	// A uniquely named directory, removed again when the object goes away.
	struct temp_directory
	{
		temp_directory()
		{
			std::random_device random_v;
			m_path = std::filesystem::temp_directory_path() / ("bootpd-test-" + std::to_string(random_v()));
			std::filesystem::create_directories(m_path);
		}

		~temp_directory()
		{
			std::error_code ec;
			std::filesystem::remove_all(m_path, ec);
		}

		temp_directory(temp_directory const&) = delete;
		auto operator = (temp_directory const&) -> temp_directory& = delete;

		auto path() const -> std::filesystem::path const& { return m_path; }

		auto write(std::filesystem::path const& relative, std::span<const std::byte> content) const -> std::filesystem::path
		{
			const auto full_v = m_path / relative;
			std::filesystem::create_directories(full_v.parent_path());
			std::ofstream stream_v(full_v, std::ios::binary);
			stream_v.write((const char*)content.data(), (std::streamsize)content.size());
			return full_v;
		}

	private:
		std::filesystem::path m_path;
	};

	// Runs `work` on `context` and stops the context (i.e. the servers) once
	// it finishes. Gives up after `limit`, so a hanging test fails instead of
	// blocking forever. Returns false on timeout.
	inline auto run_until_done(async::io_context& context, async::task<void> work,
		std::chrono::seconds limit = std::chrono::seconds(20)) -> bool
	{
		const auto wrapper = [](async::io_context& context, async::task<void> work, bool& done) -> async::task<void>
		{
			try
			{
				co_await std::move(work);
			}
			catch (...)
			{
				done = true;
				context.stop();
				throw;
			}
			done = true;
			context.stop();
		};

		bool done = false;
		context.spawn(wrapper(context, std::move(work), done));
		const auto deadline = std::chrono::steady_clock::now() + limit;
		context.run([deadline] { return std::chrono::steady_clock::now() > deadline; });
		return done;
	}
}
