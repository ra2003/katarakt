#include "layout.h"
#include "resourcemanager.h"
#include "grid.h"
#include "search.h"

using namespace std;


// TODO put in a config source file
#define USELESS_GAP 2
#define MIN_PAGE_WIDTH 150
#define MIN_ZOOM -14
#define MAX_ZOOM 30
#define ZOOM_FACTOR 0.05f

// rounds a float when afterwards cast to int
// seems to fix the mismatch between calculated page height and actual image height
#define ROUND(x) ((x) + 0.5f)


//==[ Layout ]=================================================================
Layout::Layout(ResourceManager *_res, int _page) :
		res(_res), page(_page), off_x(0), off_y(0), width(0), height(0),
		search_visible(false) {
}

int Layout::get_page() const {
	return page;
}

void Layout::rebuild() {
	// clamp to available pages
	if (page < 0) {
		page = 0;
	}
	if (page >= res->get_page_count()) {
		page = res->get_page_count() - 1;
	}
}

void Layout::resize(int w, int h) {
	width = w;
	height = h;
}

void Layout::set_zoom(int /*new_zoom*/, bool /*relative*/) {
	// implement in child classes where necessary
}

void Layout::set_columns(int /*new_columns*/, bool /*relative*/) {
	// only useful for grid layout
}

bool Layout::supports_smooth_scrolling() const {
	// normally a layout supports smooth scrolling
	return true;
}

void Layout::scroll_smooth(int dx, int dy) {
	off_x += dx;
	off_y += dy;
}

void Layout::scroll_page(int new_page, bool relative) {
	if (relative) {
		page += new_page;
	} else {
		page = new_page;
	}
	if (page < 0) {
		page = 0;
		off_y = 0;
	}
	if (page > res->get_page_count() - 1) {
		page = res->get_page_count() - 1;
	}
}

void Layout::clear_hits() {
	for (map<int,list<Result> *>::iterator it = hits.begin(); it != hits.end(); ++it) {
		delete it->second;
	}
	hits.clear();
}

void Layout::set_hits(int page, list<Result> *l) {
	// new search -> initialize highlight
	if (hits.size() == 0) {
		hit_page = page;
		hit_it = l->begin();
	}

	// just to be safe - prevent memory leaks
	map<int,list<Result> *>::iterator it = hits.find(page);
	if (it != hits.end()) {
		delete it->second;
	}
	hits[page] = l;
}

void Layout::set_search_visible(bool visible) {
	search_visible = visible;
}

void Layout::advance_hit(bool forward) {
	if (hits.size() == 0) {
		return;
	}
	// find next hit
	if (forward) {
		++hit_it;
		if (hit_it == hits[hit_page]->end()) { // this was the last hit on that page
			map<int,list<Result> *>::const_iterator it = hits.upper_bound(hit_page);
			if (it == hits.end()) { // this was the last page with a hit -> wrap
				it = hits.begin();
			}
			hit_page = it->first;
			hit_it = it->second->begin();
		}
	// find previous hit
	} else {
		if (hit_it == hits[hit_page]->begin()) { // this was the first hit on that page
			map<int,list<Result> *>::const_reverse_iterator it(hits.lower_bound(hit_page));
			if (it == hits.rend()) { // this was the first page with a hit -> wrap
				it = hits.rbegin();
			}
			hit_page = it->first;
			hit_it = --(it->second->end());
		} else {
			--hit_it;
		}
	}
}

bool Layout::get_search_visible() const {
	return search_visible;
}

//==[ PresentationLayout ]===========================================================
PresentationLayout::PresentationLayout(ResourceManager *_res, int page) :
		Layout(_res, page) {
}

PresentationLayout::PresentationLayout(Layout& old_layout) :
		Layout(old_layout) {
}

bool PresentationLayout::supports_smooth_scrolling() const {
	return false;
}

void PresentationLayout::scroll_smooth(int /*dx*/, int /*dy*/) {
	// ignore smooth scrolling
}

int PresentationLayout::calculate_fit_width(int page) {
	if ((float) width / height > res->get_page_aspect(page)) {
		return res->get_page_aspect(page) * height;
	}
	return width;
}

void PresentationLayout::render(QPainter *painter) {
	int page_width = width, page_height = height;
	int center_x = 0, center_y = 0;

	// calculate perfect fit
	if ((float) width / height > res->get_page_aspect(page)) {
		page_width = res->get_page_aspect(page) * page_height;
		center_x = (width - page_width) / 2;
	} else {
		page_height = ROUND(page_width / res->get_page_aspect(page));
		center_y = (height - page_height) / 2;
	}
	QImage *img = res->get_page(page, page_width);
	if (img != NULL) {
		if (page_width != img->width()) { // draw scaled
			QRect rect(center_x, center_y, page_width, page_height);
			painter->drawImage(rect, *img);
		} else { // draw as-is
			painter->drawImage(center_x, center_y, *img);
		}
		res->unlock_page(page);
	}

	// draw search rects
	if (search_visible) {
		painter->setPen(QColor(0, 0, 0));
		painter->setBrush(QColor(255, 0, 0, 64));
		double factor = page_width / res->get_page_width(page);
		map<int,list<Result> *>::iterator it = hits.find(page);
		if (it != hits.end()) {
			for (list<Result>::iterator i2 = it->second->begin(); i2 != it->second->end(); ++i2) {
				if (i2 == hit_it) {
					painter->setBrush(QColor(0, 255, 0, 64));
					painter->drawRect(i2->scale_translate(factor, center_x, center_y));
					painter->setBrush(QColor(255, 0, 0, 64));
				} else {
					painter->drawRect(i2->scale_translate(factor, center_x, center_y));
				}
			}
		}
	}

	// prefetch - order should be important
	if (res->get_page(page + 1, calculate_fit_width(page + 1)) != NULL) { // one after
		res->unlock_page(page + 1);
	}
	if (res->get_page(page - 1, calculate_fit_width(page - 1)) != NULL) { // one before
		res->unlock_page(page - 1);
	}
	if (res->get_page(page + 2, calculate_fit_width(page + 2)) != NULL) { // two after
		res->unlock_page(page + 2);
	}
	if (res->get_page(page - 2, calculate_fit_width(page - 2)) != NULL) { // two before
		res->unlock_page(page - 2);
	}
	res->collect_garbage(page - 4, page + 4);
}

void PresentationLayout::advance_hit(bool forward) {
	Layout::advance_hit(forward);
	scroll_page(hit_page, false);
}


//==[ SequentialLayout ]=======================================================
SequentialLayout::SequentialLayout(ResourceManager *_res, int page) :
		Layout(_res, page) {
	off_x = 0;
}

SequentialLayout::SequentialLayout(Layout& old_layout) :
		Layout(old_layout) {
	off_x = 0;
}

void SequentialLayout::scroll_smooth(int dx, int dy) {
	Layout::scroll_smooth(dx, dy);
	while (off_y <= -width / res->get_page_aspect(page) && page < res->get_page_count() - 1) {
		off_y += width / res->get_page_aspect(page);
		page++;
	}
	while (off_y > 0 && page > 0) {
		page--;
		off_y -= width / res->get_page_aspect(page);
	}
	if (page == 0 && off_y > 0) {
		off_y = 0;
	}
	off_x = 0;
}

void SequentialLayout::render(QPainter *painter) {
	int page_width = width;
	int cur_page = page, cur_offset = off_y;

	while (cur_offset < height && cur_page < res->get_page_count()) {
		int page_height = page_width / res->get_page_aspect(cur_page);
		QImage *img = res->get_page(cur_page, page_width);
		if (img != NULL) {
			painter->drawImage(off_x, cur_offset, *img);
			res->unlock_page(cur_page);
		}
		cur_offset += page_height;
		cur_page++;
	}

	// prefetch - order should be important
	if (res->get_page(cur_page, page_width) != NULL) { // one after
		res->unlock_page(cur_page);
	}
	if (res->get_page(page - 1, page_width) != NULL) { // one before
		res->unlock_page(page - 1);
	}
	if (res->get_page(cur_page + 1, page_width) != NULL) { // two after
		res->unlock_page(cur_page + 1);
	}
	if (res->get_page(page - 2, page_width) != NULL) { // two before
		res->unlock_page(page - 2);
	}

	res->collect_garbage(page - 4, cur_page + 4);
}

//==[ GridLayout ]=============================================================
GridLayout::GridLayout(ResourceManager *_res, int page, int columns) :
		Layout(_res, page),
		horizontal_page(0),
		zoom(0) {
	initialize(columns);
}

GridLayout::GridLayout(Layout& old_layout, int columns) :
		Layout(old_layout),
		horizontal_page(0),
		zoom(0) {
	initialize(columns);
}

GridLayout::~GridLayout() {
	delete grid;
}

void GridLayout::initialize(int columns) {
	grid = new Grid(res, columns);

//	size = 0.6;
//	size = 250 / grid->get_width(0);
//	size = width / grid->get_width(0);

	set_constants();
}

void GridLayout::set_constants() {
	// calculate fit
	int used = 0;
	for (int i = 0; i < grid->get_column_count(); i++) {
		used += grid->get_width(i);
	}
	int available = width - USELESS_GAP * (grid->get_column_count() - 1);
	if (available < MIN_PAGE_WIDTH * grid->get_column_count()) {
		available = MIN_PAGE_WIDTH * grid->get_column_count();
	}
	size = (float) available / used;

	// apply zoom value
	size *= (1 + zoom * ZOOM_FACTOR);

	horizontal_page = (page + horizontal_page) % grid->get_column_count();
	page = page / grid->get_column_count() * grid->get_column_count();

	total_height = 0;
	for (int i = 0; i < grid->get_row_count(); i++) {
		total_height += ROUND(grid->get_height(i * grid->get_column_count()) * size);
	}
	total_height += USELESS_GAP * (grid->get_row_count() - 1);

	total_width = 0;
	for (int i = 0; i < grid->get_column_count(); i++) {
		total_width += grid->get_width(i) * size;
	}
	total_width += USELESS_GAP * (grid->get_column_count() - 1);

	// calculate offset for blocking at the right border
	border_page_w = grid->get_column_count();
	int w = 0;
	for (int i = grid->get_column_count() - 1; i >= 0; i--) {
		w += grid->get_width(i) * size;
		if (w >= width) {
			border_page_w = i;
			border_off_w = width - w;
			break;
		}
		w += USELESS_GAP;
	}
	// bottom border
	border_page_h = grid->get_row_count() * grid->get_column_count();
	int h = 0;
	for (int i = grid->get_row_count() - 1; i >= 0; i--) {
		h += ROUND(grid->get_height(i * grid->get_column_count()) * size);
		if (h >= height) {
			border_page_h = i * grid->get_column_count();
			border_off_h = height - h;
			break;
		}
		h += USELESS_GAP;
	}

	// update view
	scroll_smooth(0, 0);
}

void GridLayout::rebuild() {
	Layout::rebuild();
	// rebuild non-dynamic data
	int columns = grid->get_column_count();
	delete grid;
	initialize(columns);
}

void GridLayout::resize(int w, int h) {
	Layout::resize(w, h);
	set_constants();
}

void GridLayout::set_zoom(int new_zoom, bool relative) {
	float old_factor = 1 + zoom * ZOOM_FACTOR;
	if (relative) {
		zoom += new_zoom;
	} else {
		zoom = new_zoom;
	}
	if (zoom < MIN_ZOOM) {
		zoom = MIN_ZOOM;
	} else if (zoom > MAX_ZOOM) {
		zoom = MAX_ZOOM;
	}
	float new_factor = 1 + zoom * ZOOM_FACTOR;

	off_x = (off_x - width / 2) * new_factor / old_factor + width / 2;
	off_y = (off_y - height / 2) * new_factor / old_factor + height / 2;

	set_constants();
}

void GridLayout::set_columns(int new_columns, bool relative) {
	if (relative) {
		grid->set_columns(grid->get_column_count() + new_columns);
	} else {
		grid->set_columns(new_columns);
	}

	set_constants();
}

void GridLayout::scroll_smooth(int dx, int dy) {
	off_x += dx;
	off_y += dy;

	// vertical scrolling
	if (total_height <= height) { // center view
		page = 0;
		off_y = (height - total_height) / 2;
	} else {
		int h; // implicit rounding
		// page up
		while (off_y > 0 &&
				(h = ROUND(grid->get_height(page - grid->get_column_count()) * size)) > 0) {
			off_y -= h + USELESS_GAP;
			page -= grid->get_column_count();
		}
		// page down
		while ((h = ROUND(grid->get_height(page) * size)) > 0 &&
				page < border_page_h &&
				off_y <= -h - USELESS_GAP) {
			off_y += h + USELESS_GAP;
			page += grid->get_column_count();
		}
		// top and bottom borders
		if (page == 0 && off_y > 0) {
			off_y = 0;
		} else if ((page == border_page_h && off_y < border_off_h) ||
				page > border_page_h) {
			page = border_page_h;
			off_y = border_off_h;
		}
	}

	// horizontal scrolling
	if (total_width <= width) { // center view
		horizontal_page = 0;
		off_x = (width - total_width) / 2;
	} else {
		int w; // implicit rounding
		// page left
		while (off_x > 0 &&
				(w = grid->get_width(horizontal_page - 1) * size) > 0) {
			off_x -= w + USELESS_GAP;
			horizontal_page--;
		}
		// page right
		while ((w = grid->get_width(horizontal_page) * size) > 0 &&
				horizontal_page < border_page_w &&
				horizontal_page < grid->get_column_count() - 1 && // only for horizontal
				off_x <= -w - USELESS_GAP) {
			off_x += w + USELESS_GAP;
			horizontal_page++;
		}
		// left and right borders
		if (horizontal_page == 0 && off_x > 0) {
			off_x = 0;
		} else if ((horizontal_page == border_page_w && off_x < border_off_w) ||
				horizontal_page > border_page_w) {
			horizontal_page = border_page_w;
			off_x = border_off_w;
		}
	}
}

void GridLayout::scroll_page(int new_page, bool relative) {
	if (total_height > height) {
		Layout::scroll_page(new_page * grid->get_column_count(), relative);
	}
	if ((page == border_page_h && off_y < border_off_h) || page > border_page_h) {
		page = border_page_h;
		off_y = border_off_h;
	}
}

void GridLayout::render(QPainter *painter) {
	// vertical
	int cur_page = page;
	int grid_height; // implicit rounding
	int hpos = off_y;
	while ((grid_height = ROUND(grid->get_height(cur_page) * size)) > 0 && hpos < height) {
		// horizontal
		int cur_col = horizontal_page;
		int grid_width; // implicit rounding
		int wpos = off_x;
		while ((grid_width = grid->get_width(cur_col) * size) > 0 &&
				cur_col < grid->get_column_count() &&
				wpos < width) {
			int page_width = res->get_page_width(cur_page + cur_col) * size;
			int page_height = ROUND(res->get_page_height(cur_page + cur_col) * size);

			int center_x = (grid_width - page_width) / 2;
			int center_y = (grid_height - page_height) / 2;

			QImage *img = res->get_page(cur_page + cur_col, page_width);
			if (img != NULL) {
/*				// debugging
				int a = img->height(), b = ROUND(grid->get_height(cur_page + cur_col) * size);
				if (a != b) {
					// TODO fix this?
					cerr << "image is " << (a - b) << " pixels bigger than expected" << endl;
				} */
				if (page_width != img->width()) { // draw scaled
					QRect rect(wpos + center_x, hpos + center_y, page_width, page_height);
					painter->drawImage(rect, *img);
				} else { // draw as-is
					painter->drawImage(wpos + center_x, hpos + center_y, *img);
				}
				res->unlock_page(cur_page + cur_col);
			}

			// draw search rects
			if (search_visible) {
				painter->setPen(QColor(0, 0, 0));
				painter->setBrush(QColor(255, 0, 0, 64));
				double factor = page_width / res->get_page_width(cur_page + cur_col);
				map<int,list<Result> *>::iterator it = hits.find(cur_page + cur_col);
				if (it != hits.end()) {
					for (list<Result>::iterator i2 = it->second->begin(); i2 != it->second->end(); ++i2) {
						if (i2 == hit_it) {
							painter->setBrush(QColor(0, 255, 0, 64));
							painter->drawRect(i2->scale_translate(factor, wpos + center_x, hpos + center_y));
							painter->setBrush(QColor(255, 0, 0, 64));
						} else {
							painter->drawRect(i2->scale_translate(factor, wpos + center_x, hpos + center_y));
						}
					}
				}
			}

			wpos += grid_width + USELESS_GAP;
			cur_col++;
		}
		hpos += grid_height + USELESS_GAP;
		cur_page += grid->get_column_count();
	}
	res->collect_garbage(page - 6, cur_page + 6);
}

void GridLayout::advance_hit(bool forward) {
	Layout::advance_hit(forward);
	// TODO improve... A LOT
	scroll_page(hit_page / grid->get_column_count(), false);
}

