/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <fstream>
#include <vector>
#include <iostream>
#include <experimental/filesystem>

#include <zlib.h>

namespace fs = std::experimental::filesystem::v1;


void trim(auto &str)
{
	while (!str.empty()) {
		if (std::isspace(str.back()))
			str.pop_back();
		else
			break;
	}
	for (auto pos(str.begin()); pos != str.end(); ++pos) {
		if (!std::isspace(*pos)) {
			str.erase(str.begin(), pos);
			break;
		}
	}
}

void byte2str(std::string &s_out, uint8_t c_in)
{
	constexpr static char nc[] = {'a', 'b', 't', 'n', 'v', 'f', 'r'};

	switch (c_in) {
	case 7 ... 13:
		s_out += '\\';
		s_out += nc[c_in - 7];
		break;
	case 32 ... 33:
		s_out += c_in;
		break;
	case 34:
	case 92:
		s_out += '\\';
		s_out += c_in;
		break;
	case 35 ... 91:
	case 93 ... 126:
		s_out += c_in;
		break;
	default:
		s_out += '\\';
		s_out += (c_in >> 6) | '0';
		s_out += ((c_in >> 3) & 7) | '0';
		s_out += (c_in & 7) | '0';
	}
}

void write_data(
	std::ofstream &out, std::string &tmp_str,
	std::vector<uint8_t> const &comp_data, std::size_t count
)
{
	std::size_t pos(0);
	while (pos < count) {
		if (tmp_str.size() > 64) {
			out << "\t\"" << tmp_str << "\"\n";
			tmp_str.clear();
		}
		byte2str(tmp_str, comp_data[pos]);
		++pos;
	}
}

int main(int argc, char **argv)
{
	if (argc < 3)
		return -1;

	fs::path out_path(argv[1]);
	fs::path in_path(argv[2]);
	std::vector<uint8_t> comp_data(1024);
	z_stream comp_state;
	comp_state.zalloc = Z_NULL;
	comp_state.zfree = Z_NULL;
	comp_state.opaque = Z_NULL;
	comp_state.avail_in = 0;
	comp_state.next_in = Z_NULL;
	deflateInit(&comp_state, 9);

	std::size_t bytes_in(0), bytes_out(0);
	std::string tmp_str;
	std::ofstream out(out_path);

	out << "/* This Source Code Form is subject to the terms of the Mozilla Public\n";
	out << " * License, v. 2.0. If a copy of the MPL was not distributed with this\n";
	out << " * file, You can obtain one at http://mozilla.org/MPL/2.0/. */\n\n";

	auto ns(out_path);
	while (!ns.extension().empty())
		ns = ns.stem();

	out << "namespace js { namespace " << ns.string() << " {\n\n";
	out << "static unsigned char const data[] = {\n";

	std::ifstream in(in_path);
	for (std::string l_in; std::getline(in, l_in);) {
		if (l_in.empty())
			continue;

		if (*l_in.begin() == '#')
			continue;

		trim(l_in);
		if (l_in.empty())
			continue;

		l_in.push_back('\n');
		bytes_in += l_in.size();
		comp_state.avail_in = l_in.size();
		comp_state.next_in = (Bytef *)l_in.data();
		do {
			comp_state.avail_out = comp_data.size();
			comp_state.next_out = comp_data.data();
			deflate(&comp_state, Z_NO_FLUSH);
			auto count(comp_data.size() - comp_state.avail_out);
			bytes_out += count;
			write_data(out, tmp_str, comp_data, count);
		} while (!comp_state.avail_out);
	}

	comp_state.avail_in = 0;
	comp_state.next_in = Z_NULL;
	do {
		comp_state.avail_out = comp_data.size();
		comp_state.next_out = comp_data.data();
		deflate(&comp_state, Z_FINISH);
		auto count(comp_data.size() - comp_state.avail_out);
		bytes_out += count;
		write_data(out, tmp_str, comp_data, count);
	} while (!comp_state.avail_out);
	deflateEnd(&comp_state);

	if (!tmp_str.empty())
		out << "\t\"" << tmp_str << "\"\n";

	out << "};\n\n";
	out << "static unsigned char const * const compressedSources = reinterpret_cast<\n\tunsigned char const *\n>(data);\n\n";
	out << "uint32_t GetCompressedSize() {\n\treturn " << bytes_out << ";\n}\n\n";
	out << "uint32_t GetRawScriptsSize() {\n\treturn " << bytes_in << ";\n}\n\n}}\n";
	return 0;
}
