
#include <iomanip>
#include <regex>
#include <filesystem>
#include <fstream>

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "GemConfig.h"
#include "Logging.h"
#include "Util.h"

#define CONFIG_FILE_NAME "gem.ini"

using namespace std;
namespace fs = std::filesystem;
using namespace GemUtil;

regex re_header("^\\[(\\w+)\\]$", regex_constants::optimize | regex_constants::ECMAScript);
regex re_setting("^(\\w+)=(.+)$", regex_constants::optimize | regex_constants::ECMAScript);
regex re_palette("rgb\\((\\d+h?),(\\d+h?),(\\d+h?)\\)\\|rgb\\((\\d+h?),(\\d+h?),(\\d+h?)\\)\\|rgb\\((\\d+h?),(\\d+h?),(\\d+h?)\\)\\|rgb\\((\\d+h?),(\\d+h?),(\\d+h?)\\)", regex_constants::icase | regex_constants::optimize | regex_constants::ECMAScript);
regex re_palette_hex("([a-f0-9]{2})([a-f0-9]{2})([a-f0-9]{2})h\\|([a-f0-9]{2})([a-f0-9]{2})([a-f0-9]{2})h\\|([a-f0-9]{2})([a-f0-9]{2})([a-f0-9]{2})h\\|([a-f0-9]{2})([a-f0-9]{2})([a-f0-9]{2})h", regex_constants::icase | regex_constants::optimize | regex_constants::ECMAScript);

// Singleton
GemConfig& GemConfig::Get()
{
	static GemConfig config;
	return config;
}

GemConfig::GemConfig()
	: VSync(false)
	, NoSound(false)
	, ForceDMGMode(false)
	, PauseAfterOpen(false)
	, ResolutionScale(3.0f)
	, UpKey(SDLK_UP)
	, DownKey(SDLK_DOWN)
	, LeftKey(SDLK_LEFT)
	, RightKey(SDLK_RIGHT)
	, AKey(SDLK_a)
	, BKey(SDLK_s)
	, StartKey(SDLK_RETURN)
	, SelectKey(SDLK_RSHIFT)
	, ShowDebugger(false)
	, ShowConsole(false)
{
	Colour0 = GemPalette::Black();
	Colour1 = GemPalette::DarkGrey();
	Colour2 = GemPalette::LightGrey();
	Colour3 = GemPalette::White();

	fs::path config_path(CONFIG_FILE_NAME);

	if (fs::exists(config_path))
	{
		ifstream fin(CONFIG_FILE_NAME, ios::in);
		if (fin.good())
		{
			Load(fin);
		}
	}

	Save();
}

void GemConfig::Save()
{
#define WRITE_SETTING(name) fout << #name << "=" << this->name << endl
#define WRITE_HEX_SETTING(name) fout << hex << uppercase << #name << "=" << this->name << nouppercase << "h" << endl << dec

	ofstream fout(CONFIG_FILE_NAME, ios::out);
	if (fout.good())
	{
		fout << "[gem]" << endl;
		WRITE_SETTING(VSync);
		WRITE_SETTING(ResolutionScale);
		WRITE_SETTING(ShowDebugger);
		WRITE_SETTING(ShowConsole);

		WRITE_HEX_SETTING(UpKey);
		WRITE_HEX_SETTING(DownKey);
		WRITE_HEX_SETTING(LeftKey);
		WRITE_HEX_SETTING(RightKey);
		WRITE_HEX_SETTING(AKey);
		WRITE_HEX_SETTING(BKey);
		WRITE_HEX_SETTING(StartKey);
		WRITE_HEX_SETTING(SelectKey);

		fout << "DMGPalette=rgb(" << int(Colour0.Red) << "," << int(Colour0.Green) << "," << int(Colour0.Blue) << ")|"
			<< "rgb(" << int(Colour1.Red) << "," << int(Colour1.Green) << "," << int(Colour1.Blue) << ")|"
			<< "rgb(" << int(Colour2.Red) << "," << int(Colour2.Green) << "," << int(Colour2.Blue) << ")|"
			<< "rgb(" << int(Colour3.Red) << "," << int(Colour3.Green) << "," << int(Colour3.Blue) << ")"
			<< endl;
	}
	else
	{
		LOG_ERROR("Failed to save config file");
	}
}

void GemConfig::Load(std::istream& stream)
{
#define PARSE_BOOL(key,value,name) if (StringEquals(key, #name)) { this->name = (bool)stoi(value); continue; }
#define PARSE_INT(key,value,name) if (StringEquals(key, #name)) { this->name = ParseNumericString(value); continue; }
#define PARSE_FLOAT(key,value,name) if (StringEquals(key, #name)) { this->name = stof(value); continue; }
#define PARSE_STR(key,value,name) if (StringEquals(key, #name)) { this->name = value; continue; }

	string line;
	string section;
	smatch match;

	while (!stream.eof())
	{
		getline(stream, line);

		if (StringStartsWith(line, "#"))
			continue;

		if (regex_match(line, match, re_header))
		{
			section = match[1].str();
		}
		else if (StringEquals(section, "gem") && regex_match(line, match, re_setting))
		{
			string key = match[1].str();
			string value = match[2].str();

			if (!StringEquals(key, "DMGPalette"))
			{
				PARSE_BOOL(key, value, VSync)
				PARSE_FLOAT(key, value, ResolutionScale)
				PARSE_BOOL(key, value, ShowDebugger)
				PARSE_BOOL(key, value, ShowConsole)

				PARSE_INT(key, value, UpKey)
				PARSE_INT(key, value, DownKey)
				PARSE_INT(key, value, LeftKey)
				PARSE_INT(key, value, RightKey)
				PARSE_INT(key, value, AKey)
				PARSE_INT(key, value, BKey)
				PARSE_INT(key, value, StartKey)
				PARSE_INT(key, value, SelectKey)
			}
			else
			{
				if (regex_match(value, match, re_palette))
				{
					Colour0 = GemColour(stoi(match[1]), stoi(match[2]), stoi(match[3]));
					Colour1 = GemColour(stoi(match[4]), stoi(match[5]), stoi(match[6]));
					Colour2 = GemColour(stoi(match[7]), stoi(match[8]), stoi(match[9]));
					Colour3 = GemColour(stoi(match[10]), stoi(match[11]), stoi(match[12]));
				}
				else if (regex_match(value, match, re_palette_hex))
				{
					Colour0 = GemColour(stoi(match[1], nullptr, 16), stoi(match[2], nullptr, 16), stoi(match[3], nullptr, 16));
					Colour1 = GemColour(stoi(match[4], nullptr, 16), stoi(match[5], nullptr, 16), stoi(match[6], nullptr, 16));
					Colour2 = GemColour(stoi(match[7], nullptr, 16), stoi(match[8], nullptr, 16), stoi(match[9], nullptr, 16));
					Colour3 = GemColour(stoi(match[10], nullptr, 16), stoi(match[11], nullptr, 16), stoi(match[12], nullptr, 16));
				}

				GemPalette::ReAssign(0, Colour0);
				GemPalette::ReAssign(1, Colour1);
				GemPalette::ReAssign(2, Colour2);
				GemPalette::ReAssign(3, Colour3);
			}
		}
	}
}
