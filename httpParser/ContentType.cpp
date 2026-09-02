#include "HttpParser.hpp"

bool HttpParser::checkContentType(const std::string& value)
{
	size_t semi = value.find(";");

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
			size_t nextSemi = paramList.find(";");
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

	if(!validChar(name) || !validChar(value))
		return false;
	return true;
}