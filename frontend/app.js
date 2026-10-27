const API_BASE = '/api';
let batches = [];
let selectedBatches = new Set();
let selectionMode = 'single';
let events = [];

document.getElementById('csvFile').addEventListener('change', handleFileSelect);
document.getElementById('personalForm').addEventListener('submit', handlePersonalSubmit);
document.getElementById('peCustomTime').addEventListener('change', toggleCustomTime);

function handleFileSelect(e) {
    const file = e.target.files[0];
    if (!file) return;
    document.getElementById('fileName').textContent = file.name;
    
    const reader = new FileReader();
    reader.onload = function(event) {
        uploadCSV(event.target.result);
    };
    reader.readAsText(file);
}

async function uploadCSV(text) {
    showStatus('Uploading...', 'info');
    try {
        const res = await fetch(`${API_BASE}/upload`, {
            method: 'POST',
            headers: {'Content-Type': 'text/plain'},
            body: text
        });
        const data = await res.json();
        if (data.error) {
            showStatus(data.error, 'error');
            return;
        }
        batches = data.batches;
        renderBatchList();
        showStatus(`Loaded ${data.totalEvents} events from ${batches.length} batch(es)`, 'success');
        if (data.skipped && data.skipped.length > 0) {
            renderWarnings(data.skipped);
        }
        showStep(2);
    } catch (err) {
        showStatus('Upload failed: ' + err.message, 'error');
    }
}

function showStatus(msg, type) {
    const el = document.getElementById('uploadStatus');
    el.textContent = msg;
    el.className = 'status ' + type;
}

function renderWarnings(warnings) {
    const list = document.getElementById('warningsList');
    list.innerHTML = warnings.map(w => `<li>${escapeHtml(w)}</li>`).join('');
    document.getElementById('warningsBox').hidden = false;
}

function renderBatchList() {
    const container = document.getElementById('batchList');
    container.innerHTML = batches.map((b, i) => `
        <label class="batch-item">
            <input type="checkbox" value="${escapeHtml(b)}" ${selectedBatches.has(b) ? 'checked' : ''} onchange="toggleBatch('${escapeHtml(b)}')">
            <span>${escapeHtml(b)}</span>
        </label>
    `).join('');
}

function filterBatches() {
    const filter = document.getElementById('batchFilter').value.toLowerCase();
    document.querySelectorAll('.batch-item').forEach(item => {
        const text = item.textContent.toLowerCase();
        item.style.display = text.includes(filter) ? 'flex' : 'none';
    });
}

function toggleBatch(batch) {
    if (selectedBatches.has(batch)) {
        selectedBatches.delete(batch);
    } else {
        selectedBatches.add(batch);
    }
}

function applySelection() {
    if (selectedBatches.size === 0) {
        alert('Select at least one batch');
        return;
    }
    selectionMode = document.querySelector('input[name="mode"]:checked').value;
    fetch(`${API_BASE}/select`, {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({batches: Array.from(selectedBatches), mode: selectionMode})
    })
    .then(r => r.json())
    .then(data => {
        events = data.events;
        renderEventsTable();
        renderSelectionClashes(data.selectionClashes);
        updatePersonalFormState();
        populateStartSlots();
        showStep(3);
        showStep(4);
        showStep(5);
    });
}

function renderSelectionClashes(clashes) {
    const box = document.getElementById('selectionClashes');
    if (!clashes || clashes.length === 0) {
        box.hidden = true;
        return;
    }
    box.hidden = false;
    box.innerHTML = '<h3>Timetable Clashes (Single Mode)</h3>' + clashes.map(c => `
        <div class="clash-item">
            ${formatEvent(c.event1)} <br> clashes with <br> ${formatEvent(c.event2)}
        </div>
    `).join('');
}

function formatEvent(e) {
    const days = ['Mon','Tue','Wed','Thu','Fri','Sat','Sun'];
    return `${days[e.day]} ${minutesToTime(e.startMin)}-${minutesToTime(e.endMin)} ${e.subject} (${e.batch})`;
}

function updatePersonalFormState() {
    const msg = document.getElementById('noBatchMsg');
    const form = document.getElementById('personalForm');
    if (selectedBatches.size === 0) {
        msg.hidden = false;
        form.querySelector('button').disabled = true;
    } else {
        msg.hidden = true;
        form.querySelector('button').disabled = false;
    }
}

function populateStartSlots() {
    const select = document.getElementById('peStartSlot');
    const slots = [
        {val: '09:00', label: '9:00 AM'},
        {val: '10:00', label: '10:00 AM'},
        {val: '11:00', label: '11:00 AM'},
        {val: '12:00', label: '12:00 PM'},
        {val: '13:00', label: '1:00 PM'},
        {val: '14:00', label: '2:00 PM'},
        {val: '15:00', label: '3:00 PM'},
        {val: '16:00', label: '4:00 PM'},
        {val: '17:00', label: '5:00 PM'}
    ];
    select.innerHTML = slots.map(s => `<option value="${s.val}">${s.label}</option>`).join('');
}

function toggleCustomTime(e) {
    document.getElementById('customTimeFields').hidden = !e.target.checked;
}

async function handlePersonalSubmit(e) {
    e.preventDefault();
    if (selectedBatches.size === 0) return;
    
    const custom = document.getElementById('peCustomTime').checked;
    let start, end, day;
    
    if (custom) {
        const startStr = document.getElementById('peCustomStart').value;
        const endStr = document.getElementById('peCustomEnd').value;
        if (!startStr || !endStr) { alert('Enter start and end time'); return; }
        day = parseInt(document.getElementById('peDay').value);
        start = timeToMinutes(startStr);
        end = timeToMinutes(endStr);
    } else {
        day = parseInt(document.getElementById('peDay').value);
        const slot = document.getElementById('peStartSlot').value;
        const numSlots = parseInt(document.getElementById('peNumSlots').value);
        start = timeToMinutes(slot);
        end = start + numSlots * 60 - 5;
    }
    
    const eventData = {
        subject: document.getElementById('peSubject').value,
        type: document.getElementById('peType').value,
        faculty: document.getElementById('peFaculty').value,
        room: document.getElementById('peRoom').value,
        day: day,
        start: start,
        end: end
    };
    
    try {
        const res = await fetch(`${API_BASE}/events`, {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(eventData)
        });
        const data = await res.json();
        if (res.status === 409) {
            renderConflict(data);
            return;
        }
        if (res.status >= 400) {
            alert(data.error || 'Failed to add event');
            return;
        }
        events.push(data.event);
        renderEventsTable();
        document.getElementById('personalForm').reset();
        document.getElementById('addConflict').hidden = true;
    } catch (err) {
        alert('Error: ' + err.message);
    }
}

function renderConflict(data) {
    const box = document.getElementById('addConflict');
    box.hidden = false;
    box.innerHTML = '<h3>Conflict Detected</h3>' +
        '<p>Clashes with:</p>' +
        data.conflicts.map(c => `<div class="conflict-item">${formatEvent(c)}</div>`).join('') +
        '<p>Suggested slots:</p>' +
        data.suggestions.map(s => `<span class="suggestion-item" onclick="useSuggestion('${s}')">${s}</span>`).join('');
}

function useSuggestion(s) {
    const match = s.match(/(\w+) (\d+:\d+)-(\d+:\d+)/);
    if (!match) return;
    const dayMap = {Mon:0, Tue:1, Wed:2, Thu:3, Fri:4, Sat:5};
    document.getElementById('peDay').value = dayMap[match[1]];
    document.getElementById('peStartSlot').value = match[2];
    document.getElementById('peCustomTime').checked = false;
    toggleCustomTime({target: {checked: false}});
    document.getElementById('addConflict').hidden = true;
}

function renderEventsTable() {
    const tbody = document.querySelector('#eventsTable tbody');
    const sorted = [...events].sort((a, b) => a.start - b.start);
    tbody.innerHTML = sorted.map(e => `
        <tr${e.source === 'personal' && hasClash(e) ? ' class="clash-row"' : ''}>
            <td>${dayName(e.start)}</td>
            <td>${minutesToTime(e.start % 1440)}-${minutesToTime(e.end % 1440)}</td>
            <td>${escapeHtml(e.subject)}</td>
            <td>${escapeHtml(e.type)}</td>
            <td>${escapeHtml(e.faculty)}</td>
            <td>${escapeHtml(e.room)}</td>
            <td>${escapeHtml(e.batch)}</td>
            <td><span class="badge ${e.source}">${e.source}</span></td>
            <td>
                ${e.source === 'personal' ? `<button class="btn danger" onclick="deleteEvent(${e.id})">Delete</button>` : ''}
            </td>
        </tr>
    `).join('');
}

function hasClash(e) {
    return events.some(other => other.id !== e.id && 
        other.start < e.end && e.start < other.end);
}

async function deleteEvent(id) {
    if (!confirm('Delete this personal event?')) return;
    try {
        const res = await fetch(`${API_BASE}/events/${id}`, {method: 'DELETE'});
        if (res.status === 403) {
            alert('Cannot delete timetable events');
            return;
        }
        events = events.filter(e => e.id !== id);
        renderEventsTable();
    } catch (err) {
        alert('Error: ' + err.message);
    }
}

async function findCommonFree() {
    const slots = parseInt(document.getElementById('cfSlots').value);
    try {
        const res = await fetch(`${API_BASE}/common-free?slots=${slots}`);
        const data = await res.json();
        renderCommonFree(data);
    } catch (err) {
        alert('Error: ' + err.message);
    }
}

function renderCommonFree(data) {
    const box = document.getElementById('commonFreeResults');
    box.hidden = false;
    box.innerHTML = `<h3>Top ${data.suggestions.length} Suggestions (${data.totalCandidates} total candidates)</h3>` +
        data.suggestions.map(s => `
            <span class="slot-suggestion">${dayName(s.start)} ${minutesToTime(s.start % 1440)}-${minutesToTime(s.end % 1440)}</span>
        `).join('');
}

function dayName(ts) {
    const days = ['Monday','Tuesday','Wednesday','Thursday','Friday','Saturday','Sunday'];
    return days[Math.floor(ts / 1440)];
}

function minutesToTime(m) {
    const h = Math.floor(m / 60);
    const min = m % 60;
    return `${String(h).padStart(2,'0')}:${String(min).padStart(2,'0')}`;
}

function timeToMinutes(str) {
    const [h, m] = str.split(':').map(Number);
    return h * 60 + m;
}

function escapeHtml(str) {
    if (!str) return '';
    return str.replace(/&/g,'&').replace(/</g,'<').replace(/>/g,'>').replace(/"/g,'"');
}

function showStep(n) {
    document.querySelectorAll('.step').forEach((s, i) => {
        s.hidden = (i + 1) > n;
        s.classList.toggle('active', i + 1 === n);
    });
}

async function loadEvents() {
    const res = await fetch(`${API_BASE}/events`);
    const data = await res.json();
    events = data.events;
    renderEventsTable();
}