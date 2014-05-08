#ifndef TAGINPUT_H
#define TAGINPUT_H

#include "multicompleter.h"
#include <QStringList>
#include <QLineEdit>
#include <QKeyEvent>

#include <unordered_map>
#include <memory>
#include <string>

namespace std {
template<>
	struct hash<QString> {
		typedef QString      argument_type;
		typedef std::size_t  result_type;
		result_type operator()(const QString &s) const {
			return std::hash<std::string>()(s.toStdString());
		}
	};
}

using StringMultimap = std::unordered_multimap<QString,QString>;

class TagInput : public QLineEdit {
	Q_OBJECT
public:
	explicit TagInput(QWidget *parent = 0);
	void fixTags(bool nosort = false);
	void loadTagFile(const QString &file);
	void reloadMappedTags();

protected:
	void keyPressEvent(QKeyEvent *event);

private:
	bool nextCompleter();
	int index;

	QStringList loadTagListFromFile();
	StringMultimap mapped_tags;
	std::unique_ptr<MultiSelectCompleter> completer;
};
#endif // TAGINPUT_H
