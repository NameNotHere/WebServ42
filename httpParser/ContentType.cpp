#include "HttpParser.hpp"

// test case: boundary=\"abc\"1\"23\";

bool HttpParser::checkContentType(const std::string& value)
{
	size_t semi = value.find(";");
	std::cout << "VALUE:" << value << "\n";

	if(value[value.size() - 1] == ';')
		return false;

	if(semi != std::string::npos)
	{
		std::string mediaType = value.substr(0, semi);

		if(!checkMediaType(mediaType))
			return false;
		std::cout << "Media type:" << mediaType << "\n";
		
		std::string paramList = value.substr(semi + 1);
		ftTrim(paramList);

		std::cout << "ParamList:" << paramList << "\n";
		if(paramList.empty())
			return false;
	
		while(!paramList.empty())
		{
			size_t nextSemi = std::string::npos;
			bool inQuotes = false;
			bool escaped = false;
			//Need to check for spaces in unquoted values
			for(size_t i = 0; i < paramList.size(); i++)
			{
				if(escaped == true)
				{
					escaped = false;
					continue;
				}
				if(paramList[i] == '\\' && inQuotes == true)
				{
					escaped = true;
					continue;
				}
				if(paramList[i] == '"')
				{
					inQuotes = !inQuotes;
				}
				if(paramList[i] == ';' && inQuotes == false)
				{
					nextSemi = i;
					break;
				}
			}
			if(inQuotes == true || escaped == true)
				return false;

			std::string param;

			if(nextSemi == std::string::npos)
			{
				param = paramList;
				paramList.clear();
			}
			else
			{
				param = paramList.substr(0, nextSemi);
				paramList = paramList.substr(nextSemi + 1);
				ftTrim(paramList);
			}
			if(!checkMediaParam(param))
				return false;
			std::cout << "Param:" << param << "\n";
		}
	}
	else
	{
		std::string mediaType = value.substr(0, semi);
		std::cout << "Media type:" << mediaType << "\n";

		if(!checkMediaType(mediaType))
			return false;
	}
	return true;
}

bool HttpParser::checkMediaType(const std::string& mediaType)
{
	size_t slash = mediaType.find("/");

	if(slash == std::string::npos)
		return false;
	
	if(slash == 0 || slash == mediaType.size() - 1)
		return false;

	if(mediaType.find("/", slash + 1) != std::string::npos)
		return false;
	
	std::string rawType = mediaType.substr(0, slash);
	std::string subType = mediaType.substr(slash + 1);

	if(!validChar(rawType) || !validChar(subType))
		return false;

	return true;
}

bool HttpParser::checkMediaParam(const std::string& param)
{
	size_t equals = param.find("=");

	if(equals == std::string::npos)
		return false;
	if (equals == 0 || equals == param.size() - 1)
    	return false;

	std::string name = param.substr(0, equals);
	std::cout << "ParamName:" << name << "\n";
	std::string value = param.substr(equals + 1);
	std::cout << "ParamValue:" << value << "\n";

	if(!validChar(name))
		return false;
	
	size_t quote = value.find('"');
	if(quote == std::string::npos)
	{
		if(!validChar(value))
			return false;
	}
	else
	{
		if(quote != 0)
			return false;

		bool inQuotes = false;
		bool escaped = false;

		for(size_t i = 0; i < value.size(); i++)
		{
			if(escaped)
			{
				escaped = false;
				continue;
			}

			if(value[i] == '\\' && inQuotes)
			{
				escaped = true;
				continue;
			}

			if(value[i] == '"')
			{
				inQuotes = !inQuotes;
				continue;
			}
			if(!inQuotes && value[i] != ' ' && value[i] != '\t')
				return false;
		}
		if(inQuotes || escaped)
			return false;
	}
	return true;
}