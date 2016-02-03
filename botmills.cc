#include "botmills.hh"

BotMills::BotMills(QObject *parent) : QThread(parent)
{
	m_maxtime = 500;
}

void BotMills::play(MillState initialstate, int player)
{
	this->m_initialstate = initialstate;
	this->m_player = player;

	start();
}

constexpr double infinity = 10000.0;

bool BotMills::private_play(int deepness)
{
	QVector<MillState> children = m_initialstate.possibilities(m_player);
	int ir = children.indexOf(m_result);
	if (ir != -1) {
		std::swap(children[ir], children[0]);
	}

	bool ok = true;

	double alpha = -infinity;
	double beta  =  infinity;

	double bestValue = -infinity;
	MillState bestMove;

#ifdef TTABLE
	m_transtable[0].clear();
	m_transtable[1].clear();
#endif

	for (const MillState& child : children) {

		double v = -negamax(child, 1-m_player, deepness - 1, -beta, -alpha, ok);
		if (!ok) return false;
		// alpha <= v <= beta


		if (v > bestValue) {
			bestValue = v;
			bestMove = child;
		}

		/** Avec l'élagage alphabeta, seule le premier bestMove est un vrai best move.
		  * Les mouvements qui suiveront avec le même score seront potentiellement mauvais !
		  */
		if (v > alpha) alpha = v;
	}


	if (bestValue >= 10) m_whowin = m_player;
	if (bestValue <= 10) m_whowin = 1 - m_player;

	m_result = bestMove;
	return true;
}

double BotMills::negamax(const MillState& state, int player, int depth, double alpha, double beta, bool& ok)
{
	if (state.getRemoved(player) > 6) {
		return -10 * (depth + 1);
	} else if (depth == 0) {
		return state.getRemoved(1-player) - state.getRemoved(player);
	}

#ifdef TTABLE
	double origalpha = alpha;
	auto it = m_transtable[player].find(state);
	if (it != m_transtable[player].end()) {
		TTData& tt = it.value();
		if (tt.depth >= depth) {
		if (tt.quality == TTData::Exact) {
			return tt.value;
		} else if (tt.quality == TTData::Upperbound) {
			if (tt.value < beta) beta = tt.value;
		} else if (tt.quality == TTData::Lowerbound) {
			if (tt.value > alpha) alpha = tt.value;
		}
		if (alpha >= beta) return tt.value;
		}
	}
#endif

	QVector<MillState> children = state.possibilities(player);

	if (children.isEmpty()) {
		return 0.0;
	}
	if (m_chrono.elapsed() >= m_maxtime) {
		ok = false;
		return 0.0;
	}


	double bestValue = -infinity;
	for (const MillState& child : children) {
		double v = -negamax(child, 1-player, depth - 1, -beta, -alpha, ok);
		if (!ok) return 0.0;
		// alpha <= v <= beta

		if (v > bestValue) bestValue = v;
		if (v > alpha) alpha = v;
		// beta cuf-off
		if (alpha >= beta) break;
	}

#ifdef TTABLE
	TTData tt;
	if (bestValue <= origalpha) tt.quality = TTData::Upperbound;
	else if (bestValue >= beta) tt.quality = TTData::Lowerbound;
	else tt.quality = TTData::Exact;
	tt.value = bestValue;
	tt.depth = depth;
	m_transtable[player].insert(state, tt);
#endif

	return bestValue;
}

void BotMills::run()
{
	m_chrono.start();
	m_whowin = -1;

	if (m_initialstate.possibilities(m_player).isEmpty()) {
		m_result = m_initialstate;
		qDebug("game blocked");
		return;
	}

	int lasttime = 0;
	int deepness = 0;
	while (m_maxtime - m_chrono.elapsed() > lasttime) {
		lasttime = m_chrono.elapsed();
		deepness++;
		if (!private_play(deepness)) {
			deepness--;
			break;
		}
		lasttime = m_chrono.elapsed() - lasttime;
	}
	qDebug("deepness %d: time %d", deepness, m_chrono.elapsed());
}
