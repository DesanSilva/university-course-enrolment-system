// State
let currentUser = null; // { userId, displayName, role, profileId }
let currentOfferingId = null; // Used for passing state between Faculty views
let lastNotifCount = 0; // Tracks seen notification count for badge

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
    },
    faculty: {
        getOfferings: (id, params = '') => api.request(`/faculty/${id}/offerings${params}`),
        getRoster: (id, offeringId) => api.request(`/faculty/${id}/offerings/${offeringId}/roster`),
        getGrades: (id, offeringId) => api.request(`/faculty/${id}/offerings/${offeringId}/grades`),
        submitGradesDraft: (id, offeringId, grades) => api.request(`/faculty/${id}/offerings/${offeringId}/grades`, 'POST', { grades }),
        submitGradesFinal: (id, offeringId) => api.request(`/faculty/${id}/offerings/${offeringId}/grades/submit`, 'POST'),
        getRequests: (id) => api.request(`/faculty/${id}/course-change-requests`),
        submitRequest: (id, payload) => api.request(`/faculty/${id}/course-change-requests`, 'POST', payload)
    },
    admin: {
        getUsers: () => api.request('/admin/users'),
        getCourses: () => api.request('/admin/courses'),
        getPrograms: () => api.request('/admin/programs'),
        getReport: (type) => api.request(`/admin/reports/${type}`),
        deactivateUser: (userId) => api.request(`/admin/users/${userId}/deactivate`, 'POST'),
        getRequests: () => api.request('/admin/course-change-requests'),
        approveRequest: (reqId) => api.request(`/admin/course-change-requests/${reqId}/approve`, 'POST'),
        rejectRequest: (reqId) => api.request(`/admin/course-change-requests/${reqId}/reject`, 'POST')
    }
};

// --- UI Utilities ---
window.showToast = (message, type = 'info') => {
    const container = document.getElementById('toast-container');
    if (!container) return;
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.innerHTML = `<span>${message}</span>`;
    container.appendChild(toast);
    setTimeout(() => {
        toast.style.opacity = '0';
        setTimeout(() => toast.remove(), 300);
    }, 3000);
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

    // Notifications bell
    const notifBtn   = document.getElementById('notifications-btn');
    const notifPanel = document.getElementById('notifications-panel');
    const notifClose = document.getElementById('notif-close-btn');
    notifBtn?.addEventListener('click', (e) => {
        e.stopPropagation();
        notifPanel.classList.toggle('hidden');
        document.getElementById('notif-badge').classList.add('hidden');
        lastNotifCount = document.getElementById('notifications-list').children.length || lastNotifCount;
    });
    notifClose?.addEventListener('click', () => notifPanel.classList.add('hidden'));
    document.addEventListener('click', (e) => {
        if (notifPanel && !notifPanel.contains(e.target) && e.target !== notifBtn) {
            notifPanel.classList.add('hidden');
        }
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
    startNotificationPolling();
    
    let targetHash = window.location.hash.substring(1);
    if (!targetHash) {
        targetHash = currentUser.role === 'ADMINISTRATOR' ? 'admin-dashboard' : 'dashboard';
        if (currentUser.role === 'FACULTY') targetHash = 'offerings';
        window.location.hash = `#${targetHash}`;
    } else {
        handleRoute();
    }
}

// --- Notification Polling ---
let notifPollInterval = null;

function startNotificationPolling() {
    if (notifPollInterval) clearInterval(notifPollInterval);
    pollNotifications();
    notifPollInterval = setInterval(pollNotifications, 10000);
}

async function pollNotifications() {
    try {
        const res  = await fetch('/api/v1/notifications');
        const json = await res.json();
        if (!json.ok) return;
        const alerts = json.data?.items ?? [];
        const list   = document.getElementById('notifications-list');
        if (!list) return;

        list.innerHTML = alerts.length
            ? alerts.slice().reverse().map(a => `
                <li class="notif-item">
                    <span class="notif-type">${a.type}</span>
                    <p>${a.message}</p>
                    <small style="color:var(--color-muted);">${new Date(a.timestampMs).toLocaleTimeString()}</small>
                </li>`).join('')
            : '<li class="notif-item" style="color:var(--color-muted);">No notifications yet.</li>';

        if (alerts.length > lastNotifCount) {
            const newAlerts = alerts.slice(lastNotifCount);
            newAlerts.forEach(a => showToast(a.message, a.type === 'SYSTEM' ? 'error' : 'info'));
            const badge = document.getElementById('notif-badge');
            if (badge) {
                badge.textContent = alerts.length - lastNotifCount;
                badge.classList.remove('hidden');
            }
        }
        lastNotifCount = alerts.length;
    } catch (_) {
        // Polling failures are silent
    }
}

// --- Navigation & Routing ---
const navConfig = {
    STUDENT: [
        { id: 'dashboard', text: 'Dashboard', view: 'student-dashboard' },
        { id: 'catalogue', text: 'Catalogue', view: 'student-catalogue' },
        { id: 'schedule', text: 'Schedule', view: 'student-schedule' },
        { id: 'progress', text: 'Progress', view: 'student-progress' },
        { id: 'waitlist', text: 'Waitlist', view: 'student-waitlist' }
    ],
    FACULTY: [
        { id: 'offerings', text: 'My Offerings', view: 'faculty-offerings' },
        { id: 'faculty-requests', text: 'Change Requests', view: 'faculty-requests' },
        { id: 'roster', view: 'faculty-roster', hidden: true },
        { id: 'grades', view: 'faculty-grades', hidden: true }
    ],
    ADMINISTRATOR: [
        { id: 'admin-dashboard', text: 'Dashboard', view: 'admin-dashboard' },
        { id: 'admin-users', text: 'Users', view: 'admin-users' },
        { id: 'admin-courses', text: 'Courses', view: 'admin-courses' },
        { id: 'admin-requests', text: 'Requests', view: 'admin-requests' },
        { id: 'admin-reports', text: 'Reports', view: 'admin-reports' }
    ]
};

function renderSidebar() {
    const items = navConfig[currentUser.role] || [];
    sidebarNav.innerHTML = items.filter(i => !i.hidden).map(item => `
        <a href="#${item.id}" class="nav-item" data-id="${item.id}">
            <span>${item.text}</span>
        </a>
    `).join('');
}

window.handleRoute = (overrideHash = null) => {
    if (!currentUser) return;
    if (typeof overrideHash === 'string') {
        window.location.hash = `#${overrideHash}`;
        return;
    }
    
    const hash = window.location.hash.substring(1);
    const items = navConfig[currentUser.role] || [];
    const route = items.find(i => i.id === hash) || items[0];
    if (!route) return;
    
    document.querySelectorAll('.nav-item').forEach(el => {
        el.classList.toggle('active', el.dataset.id === route.id || 
            (route.id === 'roster' && el.dataset.id === 'offerings') ||
            (route.id === 'grades' && el.dataset.id === 'offerings'));
    });
    
    document.querySelectorAll('.sub-view').forEach(el => el.classList.add('hidden'));
    const viewEl = document.getElementById(route.view);
    if (viewEl) viewEl.classList.remove('hidden');
    
    loadViewData(route.id);
};

// --- View Rendering ---
async function loadViewData(viewId) {
    const pid = currentUser.profileId;
    try {
        if (viewId === 'dashboard') {
            const [sched, wait] = await Promise.all([
                api.student.getSchedule(pid, '2026S1').catch(()=>({items:[]})),
                api.student.getWaitlist(pid).catch(()=>({items:[]}))
            ]);
            document.getElementById('stat-enrolled').textContent = sched.items?.length || 0;
            document.getElementById('stat-waitlisted').textContent = wait.items?.length || 0;
        }
        else if (viewId === 'catalogue') setupCatalogue(pid);
        else if (viewId === 'schedule') setupSchedule(pid);
        else if (viewId === 'progress') renderProgress(await api.student.getProgress(pid));
        else if (viewId === 'waitlist') renderWaitlist((await api.student.getWaitlist(pid)).items);
        
        else if (viewId === 'offerings') {
            const data = await api.faculty.getOfferings(pid);
            renderFacultyOfferings(data.items);
        }
        else if (viewId === 'roster' && currentOfferingId) {
            const data = await api.faculty.getRoster(pid, currentOfferingId);
            renderFacultyRoster(data);
        }
        else if (viewId === 'grades' && currentOfferingId) {
            const data = await api.faculty.getGrades(pid, currentOfferingId);
            renderFacultyGrades(data);
        }
        else if (viewId === 'faculty-requests') {
            const data = await api.faculty.getRequests(pid);
            renderFacultyRequests(data.items);
        }

        else if (viewId === 'admin-users') {
            const data = await api.admin.getUsers();
            renderAdminUsers(data.items);
        }
        else if (viewId === 'admin-courses') {
            const data = await api.admin.getCourses();
            renderAdminCourses(data.items);
        }
        else if (viewId === 'admin-requests') {
            const data = await api.admin.getRequests();
            renderAdminRequests(data.items);
        }
        else if (viewId === 'admin-reports') {
            setupAdminReports();
        }
    } catch (e) {
        console.error(e);
    }
}

/* ==========================================================================
   STUDENT LOGIC
   ========================================================================== */
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
                <div class="course-card square-card">
                    <div class="course-header">
                        <div>
                            <div class="course-code">${item.course.code}</div>
                            <h3 class="course-title">${item.course.name}</h3>
                        </div>
                        <span class="badge badge-primary">${item.course.credits} Credits</span>
                    </div>
                    <div class="course-meta">
                        <span>Instructor: ${item.instructorName}</span>
                        <span>Seats: ${item.availableSeats} / ${item.totalSeats}</span>
                    </div>
                    <p style="font-size:0.85rem; margin-top:0.25rem;">${item.course.description}</p>
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

function setupSchedule(pid) {
    const btn = document.getElementById('btn-load-sched');
    const container = document.getElementById('schedule-list');
    const load = async () => {
        const sem = document.getElementById('sched-semester').value;
        container.innerHTML = '<p>Loading...</p>';
        try {
            const data = await api.student.getSchedule(pid, sem);
            container.innerHTML = data.items.map(item => `
                <div class="course-card square-card" style="flex-direction:row; align-items:center;">
                    <div style="flex:1;">
                        <div class="course-code">${item.course.code}</div>
                        <h3 class="course-title">${item.course.name}</h3>
                        <p style="font-size:0.85rem; margin-top:0.25rem;">Status: <strong>${item.status}</strong></p>
                    </div>
                    <div class="course-actions">
                        ${item.status === 'ACTIVE' ? `<button class="btn btn-danger btn-sm" onclick="dropCourse('${item.offeringId}')">Drop</button>` : ''}
                    </div>
                </div>
            `).join('');
            if (data.items.length === 0) container.innerHTML = '<p>No enrollments found for this semester.</p>';
        } catch (e) {}
    };
    btn.onclick = load;
    if (container.children.length === 0) load();
}

function renderProgress(data) {
    document.getElementById('program-info').innerHTML = `
        <h3 style="margin-bottom:0.25rem;">${data.program.name}</h3>
        <p>Department: ${data.program.department} | Required Credits: ${data.program.requiredCredits}</p>
    `;
    const tbody = document.querySelector('#completed-courses-table tbody');
    tbody.innerHTML = data.completedCourses.map(c => `
        <tr>
            <td>${c.code}</td>
            <td>${c.name}</td>
            <td>${c.credits}</td>
            <td><strong>${c.grade}</strong></td>
        </tr>
    `).join('');
    
    document.getElementById('remaining-reqs-list').innerHTML = data.remainingRequirements.map(c => `
        <li style="background-color:var(--color-raised); padding:0.6rem 0.85rem; border:1px solid var(--color-edge);">
            <strong>${c.code}</strong> ${c.name} (${c.credits} credits)
        </li>
    `).join('');
}

function renderWaitlist(items) {
    const container = document.getElementById('waitlist-list');
    container.innerHTML = items.map(item => `
        <div class="course-card square-card">
            <div class="course-header">
                <div>
                    <div class="course-code">${item.course.code}</div>
                    <h3 class="course-title">${item.course.name}</h3>
                </div>
                <span class="badge badge-warn">Position ${item.position}</span>
            </div>
            <p style="font-size:0.85rem;">Status: ${item.status}</p>
        </div>
    `).join('');
    if (items.length === 0) container.innerHTML = '<p>You are not on any waitlists.</p>';
}

window.enrollCourse = async (offeringId) => {
    try { await api.student.enroll(currentUser.profileId, offeringId); showToast('Successfully enrolled', 'success'); loadViewData('catalogue'); } catch (e) {}
};
window.dropCourse = async (offeringId) => {
    if (!confirm('Are you sure you want to drop this course?')) return;
    try { await api.student.drop(currentUser.profileId, offeringId); showToast('Successfully dropped', 'success'); loadViewData('schedule'); } catch (e) {}
};
window.waitlistCourse = async (offeringId) => {
    try { await api.student.waitlist(currentUser.profileId, offeringId); showToast('Added to waitlist', 'success'); loadViewData('catalogue'); } catch (e) {}
};

/* ==========================================================================
   FACULTY LOGIC
   ========================================================================== */
function renderFacultyOfferings(items) {
    const container = document.getElementById('faculty-offerings-list');
    container.innerHTML = items.map(item => `
        <div class="course-card square-card">
            <div class="course-header">
                <div>
                    <div class="course-code">${item.course.code}</div>
                    <h3 class="course-title">${item.course.name}</h3>
                </div>
                <span class="badge badge-primary">${item.semester}</span>
            </div>
            <div class="course-meta">
                <span>Enrolled: ${item.totalSeats - item.availableSeats} / ${item.totalSeats}</span>
            </div>
            <div class="course-actions" style="justify-content:flex-start;">
                <button class="btn btn-secondary btn-sm" onclick="viewRoster('${item.offeringId}')">Roster</button>
                <button class="btn btn-secondary btn-sm" onclick="viewGrades('${item.offeringId}')">Grades</button>
            </div>
        </div>
    `).join('');
    if (items.length === 0) container.innerHTML = '<p>No offerings found.</p>';
}

window.viewRoster = (offeringId) => {
    currentOfferingId = offeringId;
    handleRoute('roster');
};

window.viewGrades = (offeringId) => {
    currentOfferingId = offeringId;
    handleRoute('grades');
};

function renderFacultyRoster(data) {
    document.getElementById('roster-course-info').textContent = `Offering ID: ${currentOfferingId}`;
    const tbody = document.querySelector('#roster-table tbody');
    tbody.innerHTML = data.items.map(s => `
        <tr>
            <td>${s.studentId}</td>
            <td>${s.name}</td>
            <td>${s.email}</td>
            <td><span style="color: ${s.status === 'ACTIVE' ? 'var(--color-secondary)' : 'var(--color-muted)'}">${s.status}</span></td>
        </tr>
    `).join('');
    if (data.items.length === 0) tbody.innerHTML = '<tr><td colspan="4">No students enrolled.</td></tr>';
}

function renderFacultyGrades(data) {
    document.getElementById('grades-course-info').textContent = `Offering ID: ${currentOfferingId}`;
    const tbody = document.querySelector('#grades-table tbody');
    tbody.innerHTML = data.items.map(s => `
        <tr>
            <td>${s.studentId}</td>
            <td>${s.name}</td>
            <td><strong>${s.grade || 'None'}</strong></td>
            <td>
                <input type="hidden" name="studentId[]" value="${s.studentId}">
                <select name="grade[]" style="width:90px;">
                    <option value="">Select</option>
                    ${['A+', 'A', 'A-', 'B+', 'B', 'B-', 'C+', 'C', 'C-', 'D', 'F'].map(g => `<option value="${g}" ${s.grade === g ? 'selected' : ''}>${g}</option>`).join('')}
                </select>
            </td>
        </tr>
    `).join('');
    if (data.items.length === 0) tbody.innerHTML = '<tr><td colspan="4">No students to grade.</td></tr>';
}

document.getElementById('grade-submit-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    if (!currentOfferingId) return;
    const formData = new FormData(e.target);
    const studentIds = formData.getAll('studentId[]');
    const gradesValues = formData.getAll('grade[]');
    
    const payload = [];
    for (let i = 0; i < studentIds.length; i++) {
        if (gradesValues[i]) {
            payload.push({ studentId: studentIds[i], grade: gradesValues[i] });
        }
    }
    
    if (payload.length === 0) return showToast('No grades assigned', 'error');
    
    try {
        await api.faculty.submitGradesDraft(currentUser.profileId, currentOfferingId, payload);
        await api.faculty.submitGradesFinal(currentUser.profileId, currentOfferingId);
        showToast('Grades submitted and finalized successfully', 'success');
        loadViewData('grades');
    } catch (e) {}
});

function renderFacultyRequests(items) {
    const tbody = document.querySelector('#faculty-requests-table tbody');
    tbody.innerHTML = items.map(req => `
        <tr>
            <td>${req.requestId}</td>
            <td>${req.courseId}</td>
            <td>${req.changeType}</td>
            <td>${req.requestedValue}</td>
            <td><span style="color: ${req.status === 'APPROVED' ? 'var(--color-secondary)' : (req.status === 'REJECTED' ? 'var(--color-danger)' : 'var(--color-warn)')}">${req.status}</span></td>
        </tr>
    `).join('');
}

document.getElementById('faculty-request-form')?.addEventListener('submit', async (e) => {
    e.preventDefault();
    const payload = {
        courseId: document.getElementById('req-course').value,
        changeType: document.getElementById('req-type').value,
        requestedValue: document.getElementById('req-value').value
    };
    try {
        await api.faculty.submitRequest(currentUser.profileId, payload);
        showToast('Request submitted successfully', 'success');
        e.target.reset();
        loadViewData('faculty-requests');
    } catch (e) {}
});

/* ==========================================================================
   ADMIN LOGIC
   ========================================================================== */
function renderAdminUsers(items) {
    const tbody = document.querySelector('#admin-users-table tbody');
    tbody.innerHTML = items.map(u => `
        <tr>
            <td>${u.userId}</td>
            <td>${u.name}</td>
            <td>${u.email}</td>
            <td>${u.role}</td>
            <td><span style="color: ${u.status === 'ACTIVE' ? 'var(--color-secondary)' : 'var(--color-danger)'}">${u.status}</span></td>
            <td>
                ${u.status === 'ACTIVE' ? `<button class="btn btn-danger btn-sm" onclick="deactivateUser('${u.userId}')">Deactivate</button>` : ''}
            </td>
        </tr>
    `).join('');
}

function renderAdminCourses(items) {
    const tbody = document.querySelector('#admin-courses-table tbody');
    tbody.innerHTML = items.map(c => `
        <tr>
            <td><strong>${c.code}</strong></td>
            <td>${c.name}</td>
            <td>${c.department}</td>
            <td>${c.credits}</td>
        </tr>
    `).join('');
}

window.deactivateUser = async (userId) => {
    if (!confirm(`Are you sure you want to deactivate ${userId}?`)) return;
    try {
        await api.admin.deactivateUser(userId);
        showToast('User deactivated', 'success');
        loadViewData('admin-users');
    } catch (e) {}
};

function renderAdminRequests(items) {
    const tbody = document.querySelector('#admin-requests-table tbody');
    tbody.innerHTML = items.map(req => `
        <tr>
            <td>${req.facultyId}</td>
            <td>${req.courseId}</td>
            <td>${req.changeType}</td>
            <td>${req.requestedValue}</td>
            <td><span style="color: ${req.status === 'APPROVED' ? 'var(--color-secondary)' : (req.status === 'REJECTED' ? 'var(--color-danger)' : 'var(--color-warn)')}">${req.status}</span></td>
            <td>
                ${req.status === 'PENDING' ? `
                    <button class="btn btn-primary btn-sm" onclick="handleAdminRequest('${req.requestId}', 'approve')">Approve</button>
                    <button class="btn btn-danger btn-sm" onclick="handleAdminRequest('${req.requestId}', 'reject')">Reject</button>
                ` : ''}
            </td>
        </tr>
    `).join('');
}

window.handleAdminRequest = async (reqId, action) => {
    try {
        if (action === 'approve') await api.admin.approveRequest(reqId);
        else await api.admin.rejectRequest(reqId);
        showToast(`Request ${action} completed successfully`, 'success');
        loadViewData('admin-requests');
    } catch (e) {}
};

function setupAdminReports() {
    document.getElementById('btn-load-report').onclick = async () => {
        const type = document.getElementById('report-type').value;
        const container = document.getElementById('report-content');
        container.innerHTML = '<p>Loading...</p>';
        try {
            const param = type === 'capacity' ? '?minUtilization=0.8' : '';
            const data = await api.admin.getReport(type + param);
            
            if (!data.items || data.items.length === 0) {
                container.innerHTML = '<p>No data found for this report.</p>';
                return;
            }

            if (type === 'faculty-workload') {
                container.innerHTML = `
                    <table class="data-table">
                        <thead><tr><th>Faculty</th><th>Department</th><th>Offerings</th><th>Total Students</th></tr></thead>
                        <tbody>
                            ${data.items.map(f => `<tr>
                                <td><strong>${f.facultyName}</strong><br><small style="color:var(--color-muted);">${f.facultyEmail}</small></td>
                                <td>${f.department}</td>
                                <td>${f.offeringCount}</td>
                                <td>${f.totalEnrolledStudents}</td>
                            </tr>`).join('')}
                        </tbody>
                    </table>
                `;
            } else {
                container.innerHTML = `
                    <table class="data-table">
                        <thead><tr><th>Course</th><th>Instructor</th><th>Semester</th><th>Enrolled / Capacity</th><th>Utilization</th></tr></thead>
                        <tbody>
                            ${data.items.map(c => `<tr>
                                <td><strong>${c.code}</strong><br><small style="color:var(--color-muted);">${c.courseName}</small></td>
                                <td>${c.instructorName || 'None'}</td>
                                <td>${c.semester}</td>
                                <td>${c.enrolledCount} / ${c.capacity}</td>
                                <td>${(c.utilizationRate * 100).toFixed(1)}%</td>
                            </tr>`).join('')}
                        </tbody>
                    </table>
                `;
            }
        } catch (e) {
            container.innerHTML = `<p style="color:var(--color-danger);">Failed to load report.</p>`;
        }
    };
}

// Start
init();
