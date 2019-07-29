#ifndef FILESYSTEM_PATH_H_
#define FILESYSTEM_PATH_H_

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#if defined(__linux)
	#include <linux/limits.h>
#endif

namespace filesystem {
	/**
	* \brief Simple class for manipulating paths on POSIX systems
	*/
	class path {
	public:
		path()                          : m_absolute(false) {}
		path(const path &path)          : m_path(path.m_path), m_absolute(path.m_absolute) {}
		path(const char *string)        { set(string); }
		path(const std::string &string) { set(string); }
		size_t length() const           { return m_path.size(); }
		bool empty() const              { return m_path.empty(); }
		bool is_absolute() const        { return m_absolute; }

		path make_absolute() const {
			char temp[PATH_MAX];

			if (realpath(string().c_str(), temp) == NULL)
				throw std::runtime_error("Internal error in realpath(): " + std::string(strerror(errno)));

			return path(temp);
		}

		bool exists() const {
			struct stat sb;
			return stat(string().c_str(), &sb) == 0;
		}

		size_t file_size() const {
			struct stat sb;

			if (stat(string().c_str(), &sb) != 0)
				throw std::runtime_error("path::file_size(): cannot stat file \"" + string() + "\"!");

			return (size_t) sb.st_size;
		}

		bool is_directory() const {
			struct stat sb;

			if (stat(string().c_str(), &sb))
				return false;

			return S_ISDIR(sb.st_mode);
		}

		bool is_file() const {
			struct stat sb;

			if (stat(string().c_str(), &sb))
				return false;

			return S_ISREG(sb.st_mode);
		}

		std::string extension() const {
			const std::string &name = filename();
			size_t pos = name.find_last_of(".");

			if (pos == std::string::npos)
				return "";

			return name.substr(pos+1);
		}

		std::string filename() const {
			if (empty())
				return "";

			const std::string &last = m_path[m_path.size()-1];
			return last;
		}

		path parent_path() const {
			path result;
			result.m_absolute = m_absolute;

			if (m_path.empty()) {
				if (!m_absolute)
					result.m_path.push_back("..");
			} else {
				size_t until = m_path.size() - 1;

				for (size_t i = 0; i < until; ++i)
					result.m_path.push_back(m_path[i]);
			}

			return result;
		}

		path& operator/=(const path &other) {
			if (other.m_absolute)
				throw std::runtime_error("path::operator/(): expected a relative path!");

			for (size_t i=0; i<other.m_path.size(); ++i)
				m_path.push_back(other.m_path[i]);

			return *this;
		}

		path operator/(const path &other) const {
			path result(*this);
			result /= other;
			return result;
		}

		std::string string() const {
			std::ostringstream oss;

			if (m_absolute)
				oss << "/";

			for (size_t i=0; i<m_path.size(); ++i) {
				oss << m_path[i];

				if (i+1 < m_path.size())
					oss << '/';
			}

			return oss.str();
		}

		void set(const std::string &str) {
			m_path = tokenize(str, "/");
			m_absolute = !str.empty() && str[0] == '/';
		}

		path &operator=(const path &path) {
			m_path = path.m_path;
			m_absolute = path.m_absolute;
			return *this;
		}

		friend std::ostream &operator<<(std::ostream &os, const path &path) {
			os << path.string();
			return os;
		}

		bool remove_file() {
			return std::remove(string().c_str()) == 0;
		}

		bool resize_file(size_t target_length) {
			return ::truncate(string().c_str(), (off_t) target_length) == 0;
		}

		static path getcwd() {
			char temp[PATH_MAX];

			if (::getcwd(temp, PATH_MAX) == NULL)
				throw std::runtime_error("Internal error in getcwd(): " + std::string(strerror(errno)));

			return path(temp);
		}

		bool operator==(const path &p) const { return p.m_path == m_path; }
		bool operator!=(const path &p) const { return p.m_path != m_path; }

	protected:
		static std::vector<std::string> tokenize(const std::string &string, const std::string &delim) {
			std::string::size_type lastPos = 0, pos = string.find_first_of(delim, lastPos);
			std::vector<std::string> tokens;

			while (lastPos != std::string::npos) {
				if (pos != lastPos)
					tokens.push_back(string.substr(lastPos, pos - lastPos));

				lastPos = pos;

				if (lastPos == std::string::npos || lastPos + 1 == string.length())
					break;

				pos = string.find_first_of(delim, ++lastPos);
			}

			return tokens;
		}

	protected:
		std::vector<std::string> m_path;
		bool m_absolute;
	};

	inline bool exists(const path& p) {
		return p.exists();
	}

	inline bool equivalent(const path& p1, const path& p2) {
		return p1.make_absolute() == p2.make_absolute();
	}
}
#endif