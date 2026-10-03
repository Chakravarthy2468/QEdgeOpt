// Global State
let currentDataset = null;
let convergenceChartInstance = null;
let resourceChartInstance = null;

// DOM Elements
const statusIndicator = document.getElementById('status-indicator');
const statusDot = document.getElementById('status-dot');
const statusText = document.getElementById('status-text');

const btnGenerate = document.getElementById('btn-generate');
const btnRun = document.getElementById('btn-run');
const algoSelect = document.getElementById('algo-select');
const fileUpload = document.getElementById('file-upload');

const resultsCards = document.getElementById('results-cards');
const explainPanel = document.getElementById('explainability-panel');
const benchPanel = document.getElementById('benchmark-panel');
const loadingOverlay = document.getElementById('loading-overlay');
const loadingText = document.getElementById('loading-text');

// Init
document.addEventListener('DOMContentLoaded', () => {
    checkStatus();
    initCharts();

    // Event Listeners
    algoSelect.addEventListener('change', updateParamVisibility);
    btnGenerate.addEventListener('click', generateDataset);
    btnRun.addEventListener('click', runOptimization);
    
    fileUpload.addEventListener('change', (e) => {
        const file = e.target.files[0];
        if (!file) return;
        const reader = new FileReader();
        reader.onload = (ev) => uploadDataset(ev.target.result);
        reader.readAsText(file);
    });

    updateParamVisibility();
});

// UI Logic
function updateParamVisibility() {
    document.querySelectorAll('.algo-params').forEach(el => el.classList.add('hidden'));
    const sel = algoSelect.value;
    const p = document.getElementById(`params-${sel}`);
    if (p) p.classList.remove('hidden');
}

function updateStatus(isLoaded, details) {
    if (isLoaded) {
        statusDot.className = 'w-2 h-2 rounded-full bg-green-500 mr-2';
        statusText.textContent = `Dataset: ${details.num_services} Svc, ${details.num_nodes} Nodes`;
        btnRun.disabled = false;
    } else {
        statusDot.className = 'w-2 h-2 rounded-full bg-red-500 mr-2';
        statusText.textContent = 'No Dataset';
        btnRun.disabled = true;
    }
}

// API Calls
async function checkStatus() {
    try {
        const res = await fetch('/api/status');
        const data = await res.json();
        updateStatus(data.dataset_loaded, data);
        if (data.dataset_loaded) fetchDataset();
    } catch (e) {
        console.error("API not available", e);
    }
}

async function fetchDataset() {
    try {
        const res = await fetch('/api/dataset');
        currentDataset = await res.json();
    } catch (e) {
        console.error("Failed to fetch dataset", e);
    }
}

async function generateDataset() {
    const size = document.getElementById('preset-size').value;
    let params = { seed: Math.floor(Math.random() * 1000) };
    if (size === 'small') { params.num_services = 10; params.num_nodes = 4; }
    if (size === 'medium') { params.num_services = 30; params.num_nodes = 10; }
    if (size === 'large') { params.num_services = 100; params.num_nodes = 25; }

    try {
        const res = await fetch('/api/dataset/generate', {
            method: 'POST',
            body: JSON.stringify(params)
        });
        if (res.ok) {
            currentDataset = await res.json();
            updateStatus(true, { num_services: currentDataset.services.length, num_nodes: currentDataset.nodes.length });
        }
    } catch (e) {
        alert("Failed to generate dataset");
    }
}

async function uploadDataset(jsonString) {
    try {
        const res = await fetch('/api/dataset/upload', {
            method: 'POST',
            body: jsonString
        });
        if (res.ok) {
            checkStatus();
        } else {
            const err = await res.json();
            alert("Upload failed: " + err.message);
        }
    } catch (e) {
        alert("Upload error");
    }
}

function buildConfig() {
    const algo = algoSelect.value;
    const config = {
        seed: 42,
        weights: { w_communication: 1.0, w_latency: 1.0, w_fragmentation: 0.5, w_active_nodes: 0.3 }
    };
    
    if (algo === 'qiea') {
        config.qiea = {
            population_size: parseInt(document.getElementById('qiea-pop').value),
            max_generations: parseInt(document.getElementById('qiea-gen').value),
            rotation_angle: parseFloat(document.getElementById('qiea-rot').value),
            mutation_prob: 0.05,
            elite_count: 2
        };
    } else if (algo === 'ga') {
        config.ga = {
            population_size: parseInt(document.getElementById('ga-pop').value),
            max_generations: parseInt(document.getElementById('ga-gen').value),
            crossover_prob: 0.85,
            mutation_prob: 0.05,
            tournament_size: 3,
            elite_count: 2
        };
    } else if (algo === 'benchmark') {
        config.benchmark = {
            seeds: document.getElementById('bench-seeds').value.split(',').map(s => parseInt(s.trim())),
            run_qiea: true,
            run_ga: true,
            run_ffd: true
        };
    }
    return config;
}

async function runOptimization() {
    const algo = algoSelect.value;
    const endpoint = algo === 'benchmark' ? '/api/benchmark' : `/api/optimize/${algo}`;
    
    loadingOverlay.classList.remove('hidden');
    loadingText.textContent = algo === 'benchmark' ? 'Running Benchmark...' : `Running ${algo.toUpperCase()}...`;
    
    try {
        const res = await fetch(endpoint, {
            method: 'POST',
            body: JSON.stringify(buildConfig())
        });
        const data = await res.json();
        
        loadingOverlay.classList.add('hidden');
        
        if (res.ok) {
            if (algo === 'benchmark') {
                renderBenchmark(data);
            } else {
                renderSolution(data);
            }
        } else {
            alert("Error: " + data.message);
        }
    } catch (e) {
        loadingOverlay.classList.add('hidden');
        alert("Network error");
    }
}

// Rendering
function renderSolution(data) {
    resultsCards.classList.remove('hidden');
    explainPanel.classList.remove('hidden');
    benchPanel.classList.add('hidden');

    document.getElementById('res-fitness').textContent = data.fitness.total_fitness.toFixed(2);
    document.getElementById('res-runtime').textContent = data.runtime_ms.toFixed(1) + ' ms';
    document.getElementById('res-nodes').textContent = data.fitness.active_nodes;
    
    const statusEl = document.getElementById('res-status');
    statusEl.textContent = data.status;
    statusEl.className = `text-2xl font-semibold mt-1 ${data.status === 'FEASIBLE' ? 'text-green-400' : 'text-red-400'}`;

    // Table
    const tbody = document.getElementById('placement-tbody');
    tbody.innerHTML = '';
    
    data.fitness.explanations.forEach((ex, idx) => {
        const svc = currentDataset.services[idx];
        const tr = document.createElement('tr');
        tr.innerHTML = `
            <td class="px-4 py-2 font-mono text-accent">${ex.service_id}</td>
            <td class="px-4 py-2 font-mono">${ex.node_id}</td>
            <td class="px-4 py-2">${svc.cpu} units</td>
            <td class="px-4 py-2">${svc.ram} GB</td>
            <td class="px-4 py-2 ${ex.cpu_feasible && ex.ram_feasible && ex.sla_met ? 'text-green-400' : 'text-red-400'} text-xs">
                ${ex.reason}
            </td>
        `;
        tbody.appendChild(tr);
    });

    // Charts
    updateConvergenceChart(data.convergence);
    updateResourceChart(data.fitness.node_usages);
}

function renderBenchmark(data) {
    resultsCards.classList.add('hidden');
    explainPanel.classList.add('hidden');
    benchPanel.classList.remove('hidden');

    const tbody = document.getElementById('benchmark-tbody');
    tbody.innerHTML = '';
    
    let datasets = [];
    
    data.algorithms.forEach(algo => {
        const tr = document.createElement('tr');
        tr.innerHTML = `
            <td class="px-4 py-2 font-bold">${algo.algorithm}</td>
            <td class="px-4 py-2">${algo.avg_fitness.toFixed(2)}</td>
            <td class="px-4 py-2 text-${algo.feasible_rate > 0.9 ? 'green' : 'yellow'}-400">${(algo.feasible_rate * 100).toFixed(0)}%</td>
            <td class="px-4 py-2">${algo.avg_runtime_ms.toFixed(1)}</td>
            <td class="px-4 py-2">${algo.avg_active_nodes.toFixed(1)}</td>
            <td class="px-4 py-2">${algo.avg_sla_violations.toFixed(1)}</td>
        `;
        tbody.appendChild(tr);

        if (algo.best_convergence) {
            let color = '#fbbf24'; // ffd
            if (algo.algorithm === 'QIEA') color = '#c084fc';
            if (algo.algorithm === 'Classical GA') color = '#f472b6';
            
            datasets.push({
                label: algo.algorithm,
                data: algo.best_convergence,
                borderColor: color,
                tension: 0.1,
                fill: false,
                pointRadius: 0
            });
        }
    });

    if (convergenceChartInstance) {
        convergenceChartInstance.data.labels = Array.from({length: datasets[0].data.length}, (_, i) => i + 1);
        convergenceChartInstance.data.datasets = datasets;
        convergenceChartInstance.update();
    }
}

// Charting
function initCharts() {
    Chart.defaults.color = '#94a3b8';
    Chart.defaults.borderColor = '#334155';
    
    const ctxConv = document.getElementById('convergenceChart').getContext('2d');
    convergenceChartInstance = new Chart(ctxConv, {
        type: 'line',
        data: { labels: [], datasets: [] },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: { legend: { position: 'top', align: 'end' } },
            scales: {
                y: { type: 'logarithmic', title: { display: true, text: 'Fitness (Log)' } },
                x: { title: { display: true, text: 'Generation' } }
            }
        }
    });

    const ctxRes = document.getElementById('resourceChart').getContext('2d');
    resourceChartInstance = new Chart(ctxRes, {
        type: 'bar',
        data: { labels: [], datasets: [] },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: { y: { max: 100, title: { display: true, text: '% Utilisation' } } }
        }
    });
}

function updateConvergenceChart(convergenceData) {
    if (!convergenceData || !convergenceChartInstance) return;
    convergenceChartInstance.data.labels = Array.from({length: convergenceData.length}, (_, i) => i + 1);
    convergenceChartInstance.data.datasets = [{
        label: 'Best Fitness',
        data: convergenceData,
        borderColor: '#38bdf8',
        tension: 0.1,
        fill: true,
        backgroundColor: 'rgba(56, 189, 248, 0.1)',
        pointRadius: 0
    }];
    convergenceChartInstance.update();
}

function updateResourceChart(nodeUsages) {
    if (!nodeUsages || !resourceChartInstance) return;
    
    // Filter active nodes for clearer chart
    const activeNodes = nodeUsages.filter(nu => nu.services_hosted > 0);
    
    const labels = activeNodes.map(nu => nu.node_id);
    const cpuUtil = activeNodes.map(nu => (nu.cpu_used / nu.cpu_cap) * 100);
    const ramUtil = activeNodes.map(nu => (nu.ram_used / nu.ram_cap) * 100);

    resourceChartInstance.data.labels = labels;
    resourceChartInstance.data.datasets = [
        { label: 'CPU %', data: cpuUtil, backgroundColor: '#c084fc' },
        { label: 'RAM %', data: ramUtil, backgroundColor: '#f472b6' }
    ];
    resourceChartInstance.update();
}
