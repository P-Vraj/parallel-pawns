function formatNumber(value) {
  return new Intl.NumberFormat().format(value ?? 0);
}

export default function AnalysisPanel({ analysis, engineStatus, sideToMove }) {
  const pvText = analysis.pv?.length ? analysis.pv.join(" ") : "--";

  return (
    <aside className="analysis-column">
      <div className="panel-card">
        <p className="panel-title">Evaluation</p>
        <div className="metric-box large-metric">{analysis.evaluation ?? "--"}</div>
        <p className="subtle-text">{sideToMove}</p>
      </div>

      <div className="panel-card">
        <p className="panel-title">Principal Variation</p>
        <div className="metric-box pv-box">{pvText}</div>
        <p className="subtle-text">Best move: {analysis.bestMove ?? "--"}</p>
      </div>

      <div className="panel-card">
        <p className="panel-title">Search Statistics</p>
        <div className="stats-grid">
          <div className="stat-item">
            <span>Depth</span>
            <strong>{analysis.depth ?? 0}</strong>
          </div>
          <div className="stat-item">
            <span>Nodes</span>
            <strong>{formatNumber(analysis.nodes)}</strong>
          </div>
          <div className="stat-item">
            <span>NPS</span>
            <strong>{formatNumber(analysis.nps)}</strong>
          </div>
          <div className="stat-item">
            <span>Status</span>
            <strong>{engineStatus}</strong>
          </div>
        </div>
      </div>
    </aside>
  );
}
