// State
let currentUser = null; // { userId, displayName, role, profileId }

// DOM Elements
const loginView = document.getElementById('login-view');
const appShell = document.getElementById('app-shell');
const mainContent = document.getElementById('main-content');
const sidebarNav = document.getElementById('sidebar-nav');

// --- API Service ---
const api = {
    async request(endpoint, method = 'GET', body = null) {
        const options = { method, headers: {} };
        if (body) {
            options.headers['Content-Type'] = 'application/json';
            options.body = JSON.stringify(body);
        }
        try {
            const response = await fetch(`/api/v1${endpoint}`, options);
            const data = await response.json();
            if (!response.ok) throw new Error(data.error?.message || 'Request failed');
            return data.data;
        } catch (error) {
            showToast(error.message, 'error');
            throw error;
        }
    },
    login: (userId) => api.request('/sessions', 'POST', { userId }),
    student: {
        getCatalogue: (id, params = '') => api.request(`/students/${id}/catalogue${params}`),
        getSchedule: (id, sem) => api.request(`/students/${id}/schedule?semester=${sem}`),
        getProgress: (id) => api.request(`/students/${id}/progress`),
        getWaitlist: (id) => api.request(`/students/${id}/waitlist`),
        enroll: (id, offeringId) => api.request(`/students/${id}/enrolments`, 'POST', { offeringId }),
        drop: (id, offeringId) => api.request(`/students/${id}/enrolments/${offeringId}`, 'DELETE'),
        waitlist: (id, offeringId) => api.request(`/students/${id}/waitlist`, 'POST', { offeringId })
    }
};

// --- Initialization & Auth ---
function init() {
    const saved = localStorage.getItem('nexus_user');
    if (saved) {
        currentUser = JSON.parse(saved);
        showAppShell();
    } else {
        showLogin();
    }
    
    document.getElementById('login-form').addEventListener('submit', async (e) => {
        e.preventDefault();
        const userId = document.getElementById('userId').value;
        const btn = document.getElementById('login-btn');
        const err = document.getElementById('login-error');
        btn.disabled = true;
        err.classList.add('hidden');
        try {
            const user = await api.login(userId);
            currentUser = user;
            localStorage.setItem('nexus_user', JSON.stringify(user));
            showAppShell();
        } catch (error) {
            err.textContent = error.message;
            err.classList.remove('hidden');
        } finally {
            btn.disabled = false;
        }
    });

    document.getElementById('logout-btn').addEventListener('click', () => {
        localStorage.removeItem('nexus_user');
        currentUser = null;
        window.location.hash = '';
        showLogin();
    });

    window.addEventListener('hashchange', handleRoute);
}

function showLogin() {
    loginView.classList.add('active');
    loginView.classList.remove('hidden');
    appShell.classList.add('hidden');
    appShell.classList.remove('active');
}

function showAppShell() {
    loginView.classList.add('hidden');
    loginView.classList.remove('active');
    appShell.classList.add('active');
    appShell.classList.remove('hidden');
    
    document.getElementById('user-display-name').textContent = currentUser.displayName;
    document.getElementById('user-role-badge').textContent = currentUser.role;
    
    renderSidebar();
    
    if (!window.location.hash) {
        window.location.hash = '#dashboard';
    } else {
        handleRoute();
    }
}

// --- Navigation & Routing ---
const navConfig = {
    STUDENT: [
        { id: 'dashboard', icon: 'dashboard', text: 'Dashboard', view: 'student-dashboard' },
        { id: 'catalogue', icon: 'auto_stories', text: 'Catalogue', view: 'student-catalogue' },
        { id: 'schedule', icon: 'calendar_month', text: 'Schedule', view: 'student-schedule' },
        { id: 'progress', icon: 'school', text: 'Progress', view: 'student-progress' },
        { id: 'waitlist', icon: 'hourglass_empty', text: 'Waitlist', view: 'student-waitlist' }
    ],
    FACULTY: [
        { id: 'offerings', icon: 'menu_book', text: 'My Offerings', view: 'faculty-offerings' }
    ],
    ADMINISTRATOR: [
        { id: 'admin-dash', icon: 'admin_panel_settings', text: 'Admin Panel', view: 'admin-dashboard' }
    ]
};

function renderSidebar() {
    const items = navConfig[currentUser.role] || [];
    sidebarNav.innerHTML = items.map(item => `
        <a href="#${item.id}" class="nav-item" data-id="${item.id}">
            <span class="material-symbols-outlined">${item.icon}</span>
            <span>${item.text}</span>
        </a>
    `).join('');
}

function handleRoute() {
    if (!currentUser) return;
    const hash = window.location.hash.substring(1) || 'dashboard';
    const items = navConfig[currentUser.role] || [];
    const route = items.find(i => i.id === hash) || items[0];
    
    // Update active nav
    document.querySelectorAll('.nav-item').forEach(el => {
        el.classList.toggle('active', el.dataset.id === route.id);
    });
    
    // Hide all views, show active
    document.querySelectorAll('.sub-view').forEach(el => el.classList.add('hidden'));
    const viewEl = document.getElementById(route.view);
    if (viewEl) viewEl.classList.remove('hidden');
    
    // Load view data
    loadViewData(route.id);
}

// --- View Rendering ---
async function loadViewData(viewId) {
    if (currentUser.role !== 'STUDENT') return; // Focus on student for now
    const pid = currentUser.profileId;

    try {
        if (viewId === 'dashboard') {
            const [sched, wait] = await Promise.all([
                api.student.getSchedule(pid, 'Fall 2024').catch(()=>({items:[]})),
                api.student.getWaitlist(pid).catch(()=>({items:[]}))
            ]);
            document.getElementById('stat-enrolled').textContent = sched.items?.length || 0;
            document.getElementById('stat-waitlisted').textContent = wait.items?.length || 0;
        }
        else if (viewId === 'catalogue') {
            setupCatalogue(pid);
        }
        else if (viewId === 'schedule') {
            setupSchedule(pid);
        }
        else if (viewId === 'progress') {
            const data = await api.student.getProgress(pid);
            renderProgress(data);
        }
        else if (viewId === 'waitlist') {
            const data = await api.student.getWaitlist(pid);
            renderWaitlist(data.items);
        }
    } catch (e) {
        console.error(e);
    }
}

// Student Catalogue
function setupCatalogue(pid) {
    const btn = document.getElementById('btn-search-cat');
    const container = document.getElementById('catalogue-list');
    
    const load = async () => {
        const sem = document.getElementById('cat-semester').value;
        const search = document.getElementById('cat-search').value;
        container.innerHTML = '<p>Loading...</p>';
        try {
            const params = `?semester=${encodeURIComponent(sem)}&keyword=${encodeURIComponent(search)}`;
            const data = await api.student.getCatalogue(pid, params);
            container.innerHTML = data.items.map(item => `
                <div class="course-card glass-panel">
                    <div class="course-header">
                        <div>
                            <div class="course-code">${item.course.code}</div>
                            <h3 class="course-title">${item.course.name}</h3>
                        </div>
                        <span class="badge" style="background: rgba(16, 185, 129, 0.2); color: #10b981; padding: 2px 8px; border-radius: 4px; font-size: 0.8rem;">${item.course.credits} Credits</span>
                    </div>
                    <div class="course-meta">
                        <span><span class="material-symbols-outlined" style="font-size:1rem;vertical-align:middle;">person</span> ${item.instructorName}</span>
                        <span><span class="material-symbols-outlined" style="font-size:1rem;vertical-align:middle;">chair</span> ${item.availableSeats} / ${item.totalSeats} seats</span>
                    </div>
                    <p style="font-size: 0.85rem; margin-top: 0.5rem; display: -webkit-box; -webkit-line-clamp: 2; -webkit-box-orient: vertical; overflow: hidden;">${item.course.description}</p>
                    <div class="course-actions">
                        ${item.availableSeats > 0 
                            ? `<button class="btn btn-primary btn-sm" onclick="enrollCourse('${item.offeringId}')">Enroll</button>`
                            : `<button class="btn btn-secondary btn-sm" onclick="waitlistCourse('${item.offeringId}')">Waitlist</button>`}
                    </div>
                </div>
            `).join('');
            if (data.items.length === 0) container.innerHTML = '<p>No courses found.</p>';
        } catch (e) {}
    };
    
    btn.onclick = load;
    if (container.children.length === 0) load();
}

// Student Schedule
function setupSchedule(pid) {
    const btn = document.getElementById('btn-load-sched');
    const container = document.getElementById('schedule-list');
    
    const load = async () => {
        const sem = document.getElementById('sched-semester').value;
        container.innerHTML = '<p>Loading...</p>';
        try {
            const data = await api.student.getSchedule(pid, sem);
            container.innerHTML = data.items.map(item => `
                <div class="course-card glass-panel" style="flex-direction: row; align-items: center;">
                    <div style="flex: 1;">
                        <div class="course-code">${item.course.code}</div>
                        <h3 class="course-title">${item.course.name}</h3>
                        <p style="font-size: 0.85rem; margin-top: 0.25rem;">Status: <strong>${item.status}</strong></p>
                    </div>
                    <div class="course-actions">
                        ${item.status === 'ACTIVE' ? `<button class="btn btn-secondary btn-sm" style="color:#ef4444;" onclick="dropCourse('${item.offeringId}')">Drop</button>` : ''}
                    </div>
                </div>
            `).join('');
            if (data.items.length === 0) container.innerHTML = '<p>No enrollments found for this semester.</p>';
        } catch (e) {}
    };
    btn.onclick = load;
    if (container.children.length === 0) load();
}

// Progress
function renderProgress(data) {
    document.getElementById('program-info').innerHTML = `
        <h3>${data.program.name}</h3>
        <p>Department: ${data.program.department} | Required Credits: ${data.program.requiredCredits}</p>
    `;
    const tbody = document.querySelector('#completed-courses-table tbody');
    tbody.innerHTML = data.completedCourses.map(c => `
        <tr>
            <td>${c.code}</td>
            <td>${c.name}</td>
            <td>${c.credits}</td>
            <td>${c.grade}</td>
        </tr>
    `).join('');
    
    document.getElementById('remaining-reqs-list').innerHTML = data.remainingRequirements.map(c => `
        <li><strong>${c.code}</strong> - ${c.name} (${c.credits} cr)</li>
    `).join('');
}

function renderWaitlist(items) {
    const container = document.getElementById('waitlist-list');
    container.innerHTML = items.map(item => `
        <div class="course-card glass-panel">
            <div class="course-header">
                <div>
                    <div class="course-code">${item.course.code}</div>
                    <h3 class="course-title">${item.course.name}</h3>
                </div>
                <span class="badge" style="background: rgba(245, 158, 11, 0.2); color: #f59e0b; padding: 2px 8px; border-radius: 4px;">Pos: ${item.position}</span>
            </div>
            <p style="font-size: 0.85rem; margin-top: 0.25rem;">Status: ${item.status}</p>
            <div class="course-actions" style="margin-top: 1rem;">
                <button class="btn btn-secondary btn-sm" onclick="dropCourse('${item.offeringId}')">Remove</button>
            </div>
        </div>
    `).join('');
    if (items.length === 0) container.innerHTML = '<p>You are not on any waitlists.</p>';
}

// --- Actions ---
window.enrollCourse = async (offeringId) => {
    try {
        await api.student.enroll(currentUser.profileId, offeringId);
        showToast('Successfully enrolled!', 'success');
        loadViewData('catalogue');
    } catch (e) {}
};

window.dropCourse = async (offeringId) => {
    if (!confirm('Are you sure you want to drop this course?')) return;
    try {
        await api.student.drop(currentUser.profileId, offeringId);
        showToast('Successfully dropped.', 'success');
        loadViewData('schedule');
        loadViewData('waitlist');
    } catch (e) {}
};

window.waitlistCourse = async (offeringId) => {
    try {
        await api.student.waitlist(currentUser.profileId, offeringId);
        showToast('Added to waitlist!', 'success');
        loadViewData('catalogue');
    } catch (e) {}
};

// --- Utils ---
function showToast(msg, type = 'success') {
    const t = document.createElement('div');
    t.className = `toast ${type}`;
    t.innerHTML = `<span class="material-symbols-outlined">${type === 'success' ? 'check_circle' : 'error'}</span> <span>${msg}</span>`;
    document.getElementById('toast-container').appendChild(t);
    setTimeout(() => {
        t.style.opacity = '0';
        setTimeout(() => t.remove(), 300);
    }, 3000);
}

// Start
init();
