#ifndef SEARCH_H
#define SEARCH_H

#include <poppler/qt4/poppler-qt4.h>
#include <QString>
#include <QThread>
#include <QMutex>
#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QHBoxLayout>
#include <QRect>
#include <QEvent>
#include <QList>


class SearchBar;
class Canvas;
class Viewer;


class SearchWorker : public QThread {
	Q_OBJECT

public:
	SearchWorker(SearchBar *_bar);
	void run();

	volatile bool stop;
	volatile bool die;

private:
	SearchBar *bar;
};


class SearchBar : public QWidget {
	Q_OBJECT

public:
	SearchBar(QString file, Viewer *v, QWidget *parent = 0);
	~SearchBar();

	void load(QString &file, const QByteArray &password);
	bool is_valid() const;
	void connect_canvas(Canvas *c) const;
	void focus();

signals:
	void search_clear();
	void search_done(int page, QList<QRectF> *hits);
	void search_visible(bool visible);
	void update_label_text(const QString &text);

protected:
	// QT event handling
	bool event(QEvent *event);

private slots:
	void set_text();

private:
	void initialize(QString &file, const QByteArray &password);
	void join_threads();
	void shutdown();

	QLineEdit *line;
	QLabel *progress;
	QHBoxLayout *layout;

	Poppler::Document *doc;
	Viewer *viewer;

	QMutex search_mutex;
	QMutex term_mutex;
	SearchWorker *worker;
	QString term;
	int start_page;

	friend class SearchWorker;
};

#endif
