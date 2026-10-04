#ifndef __EDITOR_EDITOR_LANGUAGE_CATALOG_H__
#define __EDITOR_EDITOR_LANGUAGE_CATALOG_H__

class CLanguageCatalog
{
public:
    static int Count();
    static int IndexOf(const char* pCode);
    static const char* Code(int index);

    static const char* GetLabel(void* pUserData, int index);

private:
    static const char* Label(int index);
};

#endif // __EDITOR_EDITOR_LANGUAGE_CATALOG_H__