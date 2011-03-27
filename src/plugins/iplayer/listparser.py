#
# Provides a simple and very quick way to parse list feeds
#

import re

def xmlunescape (data):
    data = data.replace ('&amp;', '&')
    data = data.replace ('&gt;', '>')
    data = data.replace ('&lt;', '<')
    return data

class ListEntry (object):
    def __init__ (self, title = None, identifier = None, updated = None,
                  summary = None, categories = None):
        self.title = title
        self.identifier = identifier
        self.updated = updated
        self.summary = summary
        self.categories = categories

class ListEntries (object):
    def __init__ (self):
        self.entries = []

def parse (xml_source):
    #try:
    #    regexp = "<\?xml version=\"[^\"]*\" encoding=\" ([^\"]*)\"\?>"
    #    encoding = re.findall (regexp, xml_source)[0]
    #except:
    #    return None

    elist = ListEntries ()
    # gather all list entries
    entries_src = re.findall ("<entry> (.*?)</entry>", xml_source, re.DOTALL)
    datematch = re.compile (':\s+ ([0-9]+)/ ([0-9]+)/ ([0-9]{4})')

    # enumerate thru the element list and gather info
    for entry_src in entries_src:
        title = re.findall ("<title[^>]*> (.*?)</title>",
                            entry_src, re.DOTALL)[0]
        identifier = re.findall ("<id[^>]*> (.*?)</id>",
                                 entry_src, re.DOTALL)[0]
        updated = re.findall ("<updated[^>]*> (.*?)</updated>",
                              entry_src, re.DOTALL)[0]
        summary = re.findall ("<content[^>]*> (.*?)</content>",
                              entry_src, re.DOTALL)[0].splitlines ()[-3]
        categories = re.findall ("<category[^>]*term=\" (.*?)\"[^>]*>",
                                 entry_src, re.DOTALL)

        match = datematch.search (title)
        if match:
            # if the title contains a data at the end use that as the updated
            # date YYYY-MM-DD
            updated = "%s-%s-%s" % (match.group (3), match.group (2),
                                    match.group (1))

        e_categories = []
        for category in categories:
            e_categories.append (xmlunescape (category))
        elist.entries.append (ListEntry (xmlunescape (title),
                                         xmlunescape (identifier),
                                         xmlunescape (updated),
                                         xmlunescape (summary), e_categories))

    return elist
