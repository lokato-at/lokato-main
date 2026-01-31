// === Grid Prototype ===

// Konfiguration
const rows = 3, cols = 4;
const maxOccupancy = 9;
const lockedCells = [9,10, 11];
let usedAnimals = new Set();


const initialAnimals = {};


const animalOptions = [
    '🐱','🐶','🐦','🦔','🦆','🐴','🐫','🐰',
    '🐯','🐬','🦈','🦊','🐷','🐮','🐵','🐸'
];

const gridEl = document.getElementById('grid');
const counterEl = document.getElementById('counter');
let state = Array(rows * cols).fill(null);

// === Initialisierung ===
function createGrid() {
    gridEl.innerHTML = '';

    for (let i = 0; i < rows * cols; i++) {
        const cell = document.createElement('div');
        cell.className = 'cell';
        cell.dataset.index = i;

        if (lockedCells.includes(i)) {
            const locked = document.createElement('div');
            locked.className = 'locked';
            locked.textContent = 'X';
            cell.appendChild(locked);
            state[i] = 'locked';
            gridEl.appendChild(cell);
            continue;
        }

        if (initialAnimals[i]) {
            state[i] = initialAnimals[i];
            cell.appendChild(createAnimalButton(initialAnimals[i], i));
        } else {
            cell.appendChild(createPlusButton(i));
        }

        gridEl.appendChild(cell);
    }

    refreshCounter();
}


function createPlusButton(index) {
    const btn = document.createElement('button');
    btn.className = 'plus-btn';
    btn.innerHTML = '+';
    btn.title = 'Platz hinzufügen';
    btn.addEventListener('click', e => {
        e.stopPropagation();
        openSelection(index);
    });
    return btn;
}

function createAnimalButton(emoji, index) {
    const btn = document.createElement('button');
    btn.className = 'animal-btn';
    btn.textContent = emoji;
    btn.title = 'Klicken zum Entfernen';
    btn.addEventListener('click', e => {
        e.stopPropagation();
        removeAnimal(index);
    });
    return btn;
}

function refreshCell(index) {
    const cell = gridEl.querySelector(`.cell[data-index="${index}"]`);
    if (!cell) return;

    cell.innerHTML = '';

    if (state[index] === 'locked') {
        const locked = document.createElement('div');
        locked.className = 'locked';
        locked.textContent = 'X';
        cell.appendChild(locked);
        return;
    }

    if (state[index]) {
        cell.appendChild(createAnimalButton(state[index], index));
    } else {
        cell.appendChild(createPlusButton(index));
    }

    refreshCounter();
}

function removeAnimal(index) {
    if (state[index] && state[index] !== 'locked') {
        usedAnimals.delete(state[index]);
        state[index] = null;
        refreshCell(index);
    }
}

function countOccupancy() {
    return state.filter(v => v && v !== 'locked').length;
}

function refreshCounter() {
    counterEl.textContent = 'Belegt: ' + countOccupancy();
}

// === Auswahl-Popup ===
let overlayEl = null;
let popupEl = null;

function openSelection(index) {
    if (countOccupancy() >= maxOccupancy) {
        alert(`Maximale Belegung (${maxOccupancy}) erreicht.`);
        return;
    }

    closeSelection();

    overlayEl = document.createElement('div');
    overlayEl.className = 'overlay';
    overlayEl.addEventListener('click', closeSelection);
    document.body.appendChild(overlayEl);

    popupEl = document.createElement('div');
    popupEl.className = 'popup';
    popupEl.innerHTML = `
        <h4>Tier auswählen</h4>
        <div style="font-size:13px;margin-bottom:8px;color:#555">
            Wähle ein Tier für dieses Feld.
        </div>
    `;

    const list = document.createElement('div');
    list.className = 'animals-list';

    animalOptions.forEach(animal => {
        const b = document.createElement('button');
        b.textContent = animal;

        if (usedAnimals.has(animal)) {
            b.disabled = true;
            b.style.opacity = '0.4';
            b.style.cursor = 'not-allowed';
            b.title = animal + ' ist bereits vergeben';
        } else {
            b.title = 'Platz mit ' + animal + ' belegen';
            b.addEventListener('click', e => {
                e.stopPropagation();
                selectAnimal(index, animal);
            });
        }

        list.appendChild(b);
    });

    popupEl.appendChild(list);
    document.body.appendChild(popupEl);

    document.addEventListener('keydown', onKeyDown);
}

function closeSelection() {
    if (overlayEl) overlayEl.remove();
    if (popupEl) popupEl.remove();
    overlayEl = popupEl = null;
    document.removeEventListener('keydown', onKeyDown);
}

function onKeyDown(e) {
    if (e.key === 'Escape') closeSelection();
}

function selectAnimal(index, animal) {
    state[index] = animal;
    usedAnimals.add(animal);
    refreshCell(index);
    closeSelection();
}

// === Start ===
for (let i = 0; i < rows * cols; i++) {
    if (lockedCells.includes(i)) state[i] = 'locked';
    else state[i] = initialAnimals[i] || null;
}

createGrid();
